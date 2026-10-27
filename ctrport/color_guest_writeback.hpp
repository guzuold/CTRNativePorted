// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "compute_guest_writeback.hpp"
#include "video/session_api.h"
namespace ctrport {
// Bridge actual completed RGBA8 color generation to the existing whole-public
// GPU retile provider. No CPU rendering or tiled-layout formula; unsupported
// formats remain explicit88. RG8 uses a fenced color readback and real storage
// image upload because the sampled color-copy adapter admits only RGBA8.
// R32float uses the same fenced raw-bit storage transfer when the selected
// session admits exact format4/number7; missing capability remains88.
// Exact R32float initialization through the public storage-image detiler.
// Full raw-bit comparison is required before the attachment is admitted.
inline int initialize_float_color(void* session, uint64_t color, const CtrGraphicsColorRequirementV1& c,
                                  const void* source, uint64_t bytes) {
    if (!source || c.data_format != 4 || c.number_format != 7 || c.tile_mode != 14 || c.samples != 1 || c.layers != 1 ||
        !c.pitch || !c.height || c.pitch > 4096 || c.height > 4096 || bytes != c.bytes ||
        bytes != uint64_t(c.pitch) * c.height * 4 || bytes > 32 * 1024 * 1024)
        return 88;
    CtrGraphicsImageParametersV1 p{};
    p.version = 1;
    p.size = sizeof(p);
    p.type = 9;
    p.width = p.pitch = c.pitch;
    p.height = c.height;
    p.depth = p.layers = p.levels = p.samples = 1;
    p.data_format = 4;
    p.number_format = 7;
    p.tile_mode = 14;
    p.guest_address = c.guest_address;
    uint64_t token = 0;
    int status = ctr_video_storage_image_create_v1(session, &p, &token);
    if (status) return status;
    struct Cleanup {
        void* session;
        uint64_t token;
        ~Cleanup() { ctr_video_sampled_image_release_v1(session, token); }
    } cleanup{session, token};
    CtrGraphicsImageExtentV1 extent{1, sizeof(extent), c.pitch, c.height, c.guest_address, bytes};
    status = ctr_video_sampled_image_upload_tiled_v1(session, token, source, bytes, &extent);
    if (status) return status;
    std::vector<unsigned char> linear(static_cast<size_t>(bytes)), actual(static_cast<size_t>(bytes));
    status = ctr_video_sampled_image_readback_linear_v1(session, token, linear.data(), bytes, uint64_t(c.pitch) * 4);
    if (status) return status;
    status = ctr_video_color_attachment_upload_linear_v1(session, color, linear.data(), bytes, uint64_t(c.pitch) * 4);
    if (status) return status;
    status = ctr_video_color_attachment_readback_linear_v1(session, color, actual.data(), bytes, uint64_t(c.pitch) * 4);
    if (status) return status;
    return linear == actual ? 0 : 112;
}
struct ColorGuestDownload {
    void* session = nullptr;
    uint64_t color = 0;
    CtrGraphicsColorRequirementV1 shape{};
    static int download(void* raw, const unsigned char* prior, uint64_t length, unsigned char* output,
                        uint64_t capacity) {
        if (!raw || !prior || !output) return 60;
        const auto& d = *static_cast<ColorGuestDownload*>(raw);
        const auto& c = d.shape;
        if (!d.session || !d.color || c.version != 1 || c.size != sizeof(c)) return 60;
        const uint64_t texel_bytes = c.data_format == 3 ? 2u : 4u;
        if ((c.data_format != 10 && c.data_format != 3 && c.data_format != 4) ||
            c.number_format != (c.data_format == 4 ? 7u : 0u) ||
            (c.tile_mode != 14 && !(c.tile_mode == 10 && c.data_format == 10)) || c.samples != 1 || c.layers != 1 ||
            !c.pitch || !c.height || c.pitch > 4096 || c.height > 4096 ||
            c.bytes != uint64_t(c.pitch) * c.height * texel_bytes || length != c.bytes || capacity != length)
            return 88;
        CtrGraphicsImageParametersV1 p{};
        p.version = 1;
        p.size = sizeof(p);
        p.type = 9;
        p.width = p.pitch = c.pitch;
        p.height = c.height;
        p.depth = p.layers = p.levels = p.samples = 1;
        p.data_format = c.data_format;
        p.number_format = c.number_format;
        p.tile_mode = c.tile_mode;
        p.guest_address = c.guest_address;
        uint64_t token = 0;
        int status = c.data_format != 10 ? ctr_video_storage_image_create_v1(d.session, &p, &token)
                                         : ctr_video_sampled_image_create_v1(d.session, &p, &token);
        if (status) return status;
        struct Cleanup {
            void* session;
            uint64_t token;
            ~Cleanup() { ctr_video_sampled_image_release_v1(session, token); }
        } cleanup{d.session, token};
        if (c.data_format != 10) {
            std::vector<unsigned char> linear(static_cast<size_t>(length));
            status = ctr_video_color_attachment_readback_linear_v1(d.session, d.color, linear.data(), length,
                                                                   uint64_t(c.pitch) * texel_bytes);
            if (!status)
                status = ctr_video_sampled_image_upload_linear_v1(d.session, token, linear.data(), length,
                                                                  uint64_t(c.pitch) * texel_bytes);
        } else
            status = ctr_video_color_to_sampled_v1(d.session, d.color, token);
        if (status) return status;
        CtrGraphicsImageExtentV1 extent{1, sizeof(extent), c.pitch, c.height, c.guest_address, c.bytes};
        return ctr_video_image_readback_tiled_v1(d.session, token, prior, length, &extent, output);
    }
};
}
