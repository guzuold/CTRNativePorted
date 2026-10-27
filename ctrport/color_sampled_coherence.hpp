// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
#include <vector>
#include "video_color_batch.hpp"
#include "video_image_batch.hpp"
namespace ctrport {
// Per-session completed-writer generations. Allocation and descriptor writes
// are not writers. Call begin before recording a writer, complete only after
// its real fence; a failed writer stays pending and cannot expose stale pixels.
class ColorSampledCoherence {
    struct Color {
        uint64_t token, generation = 0, word = 0;
        bool pending = false;
    };
    struct View {
        uint64_t color, image, generation;
    };
    std::vector<Color> colors_;
    std::vector<View> views_;
    bool running_ = false;
    Color* find(uint64_t token) {
        for (auto& c : colors_)
            if (c.token == token) return &c;
        return nullptr;
    }

public:
    int add(uint64_t token) {
        if (!token) return 60;
        if (find(token)) return 0;
        colors_.push_back({token});
        return 0;
    }
    int begin(uint64_t token, uint64_t word) {
        auto* c = find(token);
        if (!c) return 62;
        if (c->pending) return 91;
        if (c->generation == UINT64_MAX) return 88;
        c->pending = true;
        c->word = word;
        return 0;
    }
    int complete(uint64_t token, uint64_t word) {
        auto* c = find(token);
        if (!c) return 62;
        if (!c->pending || c->word != word) return 91;
        c->pending = false;
        ++c->generation;
        return 0;
    }
    // Validate the complete MRT writer set before mutating any generation.
    // A recording/submit/fence failure deliberately leaves every writer pending.
    int begin_many(const uint64_t* tokens, size_t count, uint64_t word) {
        if (!tokens || !count || count > 8) return 60;
        for (size_t i = 0; i < count; ++i) {
            for (size_t j = 0; j < i; ++j)
                if (tokens[i] == tokens[j]) return 60;
            const auto* c = find(tokens[i]);
            if (!c) return 62;
            if (c->pending) return 91;
            if (c->generation == UINT64_MAX) return 88;
        }
        for (size_t i = 0; i < count; ++i) {
            auto* c = find(tokens[i]);
            c->pending = true;
            c->word = word;
        }
        return 0;
    }
    int complete_many(const uint64_t* tokens, size_t count, uint64_t word) {
        if (!tokens || !count || count > 8) return 60;
        for (size_t i = 0; i < count; ++i) {
            for (size_t j = 0; j < i; ++j)
                if (tokens[i] == tokens[j]) return 60;
            const auto* c = find(tokens[i]);
            if (!c) return 62;
            if (!c->pending || c->word != word) return 91;
        }
        for (size_t i = 0; i < count; ++i) {
            auto* c = find(tokens[i]);
            c->pending = false;
            ++c->generation;
        }
        return 0;
    }
    template <class Transfer> int synchronize(uint64_t color, uint64_t image, Transfer&& transfer) {
        if (running_) return 91;
        auto* c = find(color);
        if (!c) return 62;
        if (!image) return 60;
        if (c->pending || !c->generation) return 91;
        for (const auto& v : views_)
            if (v.image == image && v.color != color) return 88;
        for (const auto& v : views_)
            if (v.image == image && v.generation == c->generation) return 0;
        running_ = true;
        struct Reset {
            bool& value;
            ~Reset() { value = false; }
        } reset{running_};
        const auto generation = c->generation;
        int status = 112;
        try {
            status = transfer(color, image);
        } catch (...) {
            return 112;
        }
        if (status) return status;
        // A callback cannot publish a generation which changed during transfer.
        c = find(color);
        if (!c || c->pending || c->generation != generation) return 91;
        for (auto& v : views_)
            if (v.image == image) {
                v.generation = generation;
                return 0;
            }
        views_.push_back({color, image, generation});
        return 0;
    }
};
// Transfer the genuine R32 attachment generation to its independently swizzled
// sampled view. Both operations fence; CPU bytes come only from GPU readback.
// Logical view rows exclude padded attachment rows. No tiled-address formula.
inline int transfer_color_sampled_view(void* session, uint64_t color, uint64_t image,
                                       const CtrGraphicsColorRequirementV1& c, const CtrGraphicsImageParametersV1& p) {
    if (c.data_format != 4) return ctr_video_color_to_sampled_v1(session, color, image);
    if (c.version != 1 || c.size != sizeof(c) || p.version != 1 || p.size != sizeof(p) || c.bytes > 32 * 1024 * 1024 ||
        c.number_format != 7 || p.data_format != 4 || p.number_format != 7 || c.samples != 1 || p.samples != 1 ||
        c.layers != 1 || p.layers != 1 || p.levels != 1 || c.tile_mode != 14 || p.tile_mode != 14 || !p.width ||
        !p.height || p.width > c.pitch || p.height > c.height || p.pitch != c.pitch || c.pitch > 4096 ||
        c.height > 4096 || c.guest_address != p.guest_address || c.bytes != uint64_t(c.pitch) * c.height * 4)
        return 88;
    const uint64_t pitch = uint64_t(c.pitch) * 4;
    std::vector<unsigned char> linear(static_cast<size_t>(c.bytes));
    const int status =
        ctr_video_color_attachment_readback_linear_v1(session, color, linear.data(), linear.size(), pitch);
    if (status) return status;
    return ctr_video_sampled_image_upload_linear_v1(session, image, linear.data(), pitch * p.height, pitch);
}
// Compare only the explicit bounded linear view after real conversion fences.
// This verifies transport, not the correctness of the upstream guest detiler.
inline int compare_color_sampled_view(void* session, uint64_t color, uint64_t image,
                                      const CtrGraphicsColorRequirementV1& c, const CtrGraphicsImageParametersV1& p,
                                      uint64_t& compared, uint64_t& differences, uint64_t& padding) {
    if (!p.width || !p.height || p.width > c.pitch || p.height > c.height || p.pitch != c.pitch || c.pitch > 4096 ||
        c.height > 4096)
        return 60;
    if (c.data_format != p.data_format || c.number_format != p.number_format || c.samples != 1 || p.samples != 1)
        return 88;
    const uint32_t texel =
        c.data_format == 3 ? 2u : (c.data_format == 10 || (c.data_format == 4 && c.number_format == 7) ? 4u : 0u);
    if (!texel) return 88;
    const uint64_t pitch = uint64_t(c.pitch) * texel;
    std::vector<unsigned char> backing(static_cast<size_t>(pitch * c.height)),
        view(static_cast<size_t>(pitch * p.height + 16), 0xa5);
    int status = ctr_video_color_attachment_readback_linear_v1(session, color, backing.data(), backing.size(), pitch);
    if (status) return status;
    status = ctr_video_sampled_image_readback_linear_v1(session, image, view.data(), view.size(), pitch);
    if (status) return status;
    for (size_t at = 0; at < view.size(); ++at) {
        const bool pixel = at / pitch < p.height && at % pitch < uint64_t(p.width) * texel;
        if (pixel) {
            ++compared;
            differences += view[at] != backing[at];
        } else
            padding += view[at] != 0xa5;
    }
    return differences || padding ? 112 : 0;
}

}
