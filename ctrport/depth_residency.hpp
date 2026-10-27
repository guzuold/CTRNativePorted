// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "gpu_resident_memory.hpp"
#include "gnm_acquire_mem.hpp"
#include "video_session_owner.hpp"
#include "video/graphics_companion.h"
namespace ctrport {
// Genuine GPU image leases, exact public logical intervals, conservative page
// exclusion. Padding is CPU exclusion only, never a stencil sample layout.
class DepthResidency {
    struct Entry {
        void* context;
        uint64_t token, lease, generation;
        uint32_t writer;
        CtrDepthBackingSpansV1 spans;
        CtrDepthMS2LayoutV1 layout;
        uint64_t guard_begin, guard_bytes;
        std::shared_ptr<void> owner;
        std::shared_ptr<const std::vector<unsigned char>> prior;
    };
    // Accessed only on the retained presentation owner thread; process teardown
    // occurs after guest workers joined. The guard owns an independent lease copy.
    std::vector<Entry> entries_;

public:
    int retain(const CtrVideoSessionApiV1* api, std::shared_ptr<RuntimeVideoSession> session, uint64_t token,
               uint32_t writer, const CtrDepthMS2LayoutV1& layout, uint64_t stencil) {
        if (!api || !api->context || !session || !token) return 60;
        CtrDepthBackingSpansV1 spans{};
        const int metadata = layout.samples == 1 ? ctr_video_depth_single_spans_v1(&layout, stencil, &spans)
                                                 : ctr_video_depth_ms2_spans_v1(&layout, stencil, &spans);
        if (metadata) return metadata;
        for (auto& entry : entries_)
            if (entry.context == api->context && entry.token == token) {
                if (entry.spans.depth_address != spans.depth_address || entry.spans.depth_bytes != spans.depth_bytes ||
                    entry.spans.stencil_address != spans.stencil_address ||
                    entry.spans.stencil_bytes != spans.stencil_bytes)
                    return 88;
                // The same allocation token cannot silently change its sampled view.
                const auto& prior = entry.layout;
                if (prior.width != layout.width || prior.height != layout.height || prior.pitch != layout.pitch ||
                    prior.samples != layout.samples || prior.z_format != layout.z_format ||
                    prior.stencil_format != layout.stencil_format || prior.tile_mode != layout.tile_mode ||
                    prior.slice_tile_max != layout.slice_tile_max || prior.slice_start != layout.slice_start ||
                    prior.slice_max != layout.slice_max || prior.alt_tile != layout.alt_tile ||
                    prior.bank_swizzle != layout.bank_swizzle)
                    return 88;
                if (entry.writer == writer) return 0;
                const int status = runtime_gpu_resident_memory.advance(
                    entry.guard_begin, entry.guard_bytes, entry.lease, entry.generation, entry.generation + 1);
                if (status) return status;
                ++entry.generation;
                entry.writer = writer;
                return 0;
            }

        // This reached backing has adjacent depth/stencil intervals. Other placement
        // must establish separately validated page ownership rather than fill a gap.
        const bool depth_only = layout.samples == 1 && layout.z_format == 1 && !layout.stencil_format;
        if (depth_only) {
            if (stencil || spans.stencil_address || spans.stencil_bytes) return 88;
        } else if (spans.depth_address + spans.depth_bytes != spans.stencil_address)
            return 88;
        if (!spans.depth_address || !spans.depth_bytes || spans.depth_address > UINT64_MAX - spans.depth_bytes)
            return 60;
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        const uint64_t page = info.dwPageSize, end = depth_only ? spans.depth_address + spans.depth_bytes
                                                                : spans.stencil_address + spans.stencil_bytes;
        if (!page || end > UINT64_MAX - (page - 1)) return 60;
        const uint64_t begin = spans.depth_address / page * page, guard_end = (end + page - 1) / page * page;
        uint64_t lease = 0;
        const int retained = ctr_video_depth_attachment_retain_v1(api->context, token, &lease);
        if (retained) return retained;
        if (!lease || lease == token) return 112;
        auto owner = std::shared_ptr<void>(new uint64_t(lease), [session](void* value) {
            const auto lease = *static_cast<uint64_t*>(value);
            try {
                session->invoke([&](const CtrVideoSessionApiV1* api) {
                    return ctr_video_depth_attachment_release_v1(api->context, lease);
                });
            } catch (...) {
                OutputDebugStringA("Depth residency lease shutdown unavailable\n");
            }
            delete static_cast<uint64_t*>(value);
        });
        try {
            entries_.reserve(entries_.size() + 1);
        } catch (...) {
            return 112;
        }
        for (auto& entry : entries_)
            if (entry.context == api->context && entry.spans.depth_address == spans.depth_address &&
                entry.spans.depth_bytes == spans.depth_bytes && entry.spans.stencil_address == spans.stencil_address &&
                entry.spans.stencil_bytes == spans.stencil_bytes) {
                const auto& prior = entry.layout;
                if (prior.width != layout.width || prior.height != layout.height || prior.pitch != layout.pitch ||
                    prior.samples != layout.samples || prior.z_format != layout.z_format ||
                    prior.stencil_format != layout.stencil_format || prior.tile_mode != layout.tile_mode ||
                    prior.slice_tile_max != layout.slice_tile_max || prior.slice_start != layout.slice_start ||
                    prior.slice_max != layout.slice_max || prior.alt_tile != layout.alt_tile ||
                    prior.bank_swizzle != layout.bank_swizzle)
                    return 88;
                const int covered = covers(api->context, entry.token, entry.writer);
                if (covered) return covered;
                const int replaced = runtime_gpu_resident_memory.replace(
                    begin, guard_end - begin, entry.lease, entry.generation, lease, entry.generation + 1, owner);
                if (replaced) return replaced;
                entry.token = token;
                entry.lease = lease;
                ++entry.generation;
                entry.writer = writer;
                entry.layout = layout;
                entry.owner = std::move(owner);
                return 0;
            }
        std::shared_ptr<std::vector<unsigned char>> prior;
        try {
            prior = std::make_shared<std::vector<unsigned char>>();
        } catch (...) {
            return 112;
        }
        // Exact shifted reuse reached after bounded compute publication: the new
        // whole GPU owner replaces ALL remaining old guard pages, never a fake suffix view.
        for (auto& entry : entries_)
            if (entry.context == api->context && begin > entry.guard_begin &&
                begin < entry.guard_begin + entry.guard_bytes) {
                uint64_t old_image = 0, old_generation = 0, old_begin = 0, old_bytes = 0;
                if (!runtime_gpu_resident_memory.identify(begin, old_image, old_generation, old_begin, old_bytes) ||
                    old_begin != begin || old_image != entry.lease || old_generation != entry.generation + 1 ||
                    old_generation == UINT64_MAX || old_begin + old_bytes != entry.guard_begin + entry.guard_bytes ||
                    guard_end <= old_begin + old_bytes || !entry.prior || entry.prior->size() != entry.guard_bytes)
                    return 88;
                if (entry.layout.width != layout.width || entry.layout.height != layout.height ||
                    entry.layout.pitch != layout.pitch || entry.layout.samples != layout.samples ||
                    entry.layout.z_format != layout.z_format || entry.layout.stencil_format != layout.stencil_format ||
                    entry.layout.tile_mode != layout.tile_mode || entry.spans.depth_bytes != spans.depth_bytes ||
                    entry.spans.stencil_bytes != spans.stencil_bytes)
                    return 88;
                std::vector<unsigned char> suffix;
                try {
                    suffix.assign(entry.prior->begin() + static_cast<size_t>(begin - entry.guard_begin),
                                  entry.prior->end());
                } catch (...) {
                    return 112;
                }
                const int replaced =
                    runtime_gpu_resident_memory.replace_extend(begin, guard_end - begin, old_image, old_generation,
                                                               lease, old_generation + 1, owner, suffix, *prior);
                if (replaced) return replaced;
                // Shifted layout padding can overlap old GPU-written logical cells.
                // Old CPU prior bytes are NOT authority for that padding; require a real
                // backing extraction before any future retile/publication of this surface.
                entry = {api->context, token,  lease, old_generation + 1, writer,
                         spans,        layout, begin, guard_end - begin,  std::move(owner),
                         nullptr};
                return 0;
            }
        const int protected_pages =
            runtime_gpu_resident_memory.protect(begin, guard_end - begin, lease, 1, owner, prior.get());
        if (protected_pages) return protected_pages;
        entries_.push_back({api->context, token, lease, 1, writer, spans, layout, begin, guard_end - begin,
                            std::move(owner), std::move(prior)});
        return 0;
    }
    int covers(void* context, uint64_t token, uint32_t writer) const {
        for (const auto& entry : entries_)
            if (entry.context == context && entry.token == token && entry.writer == writer) {
                uint64_t image = 0, generation = 0, begin = 0, bytes = 0;
                if (!entry.owner ||
                    !runtime_gpu_resident_memory.identify(entry.guard_begin, image, generation, begin, bytes) ||
                    image != entry.lease || generation != entry.generation || begin != entry.guard_begin ||
                    bytes != entry.guard_bytes)
                    return 88;
                for (uint64_t cursor = begin; cursor < begin + bytes;) {
                    MEMORY_BASIC_INFORMATION region{};
                    if (!VirtualQuery(reinterpret_cast<void*>(cursor), &region, sizeof(region)) ||
                        region.State != MEM_COMMIT || region.Protect != PAGE_NOACCESS)
                        return 88;
                    const uint64_t next = reinterpret_cast<uint64_t>(region.BaseAddress) + region.RegionSize;
                    if (next <= cursor) return 88;
                    cursor = next;
                }
                return 0;
            }
        return 88;
    }
    // GPU consumers supply exact logical plane identity, never rounded guard
    // padding. Outputs retain the actual owner only after generation validation.
    int resolve(void* context, const CtrDepthBackingSpansV1& spans, uint64_t expected, uint64_t& lease,
                std::shared_ptr<void>& owner) const {
        if (!context || spans.version != 1 || spans.size != sizeof(spans) || !expected) return 60;
        for (const auto& entry : entries_)
            if (entry.context == context && entry.spans.depth_address == spans.depth_address &&
                entry.spans.depth_bytes == spans.depth_bytes && entry.spans.stencil_address == spans.stencil_address &&
                entry.spans.stencil_bytes == spans.stencil_bytes) {
                if (entry.generation != expected) return 88;
                const int status = covers(context, entry.token, entry.writer);
                if (status) return status;
                lease = entry.lease;
                owner = entry.owner;
                return 0;
            }
        return 88;
    }
    // Resolve only a complete depth interval at the generation observed by the
    // caller. Full depth+stencil identity and retained guard are checked by resolve.
    int resolve_sampled_interval(void* context, uint64_t address, uint64_t bytes, uint64_t generation, uint64_t& lease,
                                 std::shared_ptr<void>& owner) const {
        const Entry* found = nullptr;
        for (const auto& entry : entries_)
            if (entry.context == context && entry.spans.depth_address == address && entry.spans.depth_bytes == bytes) {
                if (found) return 88;
                found = &entry;
            }
        if (!found) return 88;
        return resolve(context, found->spans, generation, lease, owner);
    }
    static bool tall_d16_view(const CtrGraphicsImageParametersV1& p, const CtrGraphicsImageExtentV1& e) {
        return p.version == 1 && p.size == sizeof(p) && e.version == 1 && e.size == sizeof(e) && p.type == 9 &&
               p.width == 2048 && p.height == 8192 && p.pitch == 2048 && p.depth == 1 && p.layers == 1 &&
               p.levels == 1 && p.samples == 1 && p.data_format == 2 && !p.number_format && !p.tile_mode &&
               !p.pow2pad && !p.base_level && !p.base_array && !p.last_array && !p.compression && !p.alt_tile &&
               p.swizzle[0] == 4 && p.swizzle[1] == 4 && p.swizzle[2] == 4 && p.swizzle[3] == 4 &&
               p.guest_address == e.guest_address && e.pitch == 2048 && e.padded_height == 8192 && e.bytes == 33554432;
    }
    // Exact D32 sampled view; backing interval alone is insufficient authority.
    int resolve_sampled_view(void* context, const CtrGraphicsImageParametersV1& p,
                             const CtrGraphicsImageExtentV1& extent, uint64_t generation, uint64_t& lease,
                             std::shared_ptr<void>& owner) const {
        if (p.version != 1 || p.size != sizeof(p) || extent.version != 1 || extent.size != sizeof(extent)) return 60;
        if (tall_d16_view(p, extent)) {
            const Entry* found = nullptr;
            for (const auto& e : entries_)
                if (e.context == context && e.spans.depth_address == p.guest_address) {
                    if (found) return 88;
                    found = &e;
                }
            if (!found) return 88;
            const auto& l = found->layout;
            if (l.width != 2048 || l.height != 8192 || l.pitch != 2048 || l.samples != 1 || l.z_format != 1 ||
                l.stencil_format || l.tile_mode || l.slice_tile_max != 262143 || l.slice_start || l.slice_max ||
                l.alt_tile || l.bank_swizzle)
                return 88;
            CtrDepthBackingSpansV1 spans{};
            const int decoded = ctr_video_depth_single_spans_v1(&l, 0, &spans);
            if (decoded) return decoded;
            if (spans.depth_bytes != extent.bytes || spans.stencil_address || spans.stencil_bytes) return 88;
            return resolve(context, spans, generation, lease, owner);
        }
        if (p.type != 14 || p.depth != 1 || p.layers != 1 || p.levels != 1 || p.samples != 2 || p.data_format != 4 ||
            p.number_format != 7 || p.tile_mode != 1 || p.pow2pad || p.base_level || p.base_array || p.last_array ||
            p.compression || p.alt_tile || p.swizzle[0] != 4 || p.swizzle[1] != 4 || p.swizzle[2] != 4 ||
            p.swizzle[3] != 4 || p.guest_address != extent.guest_address)
            return 88;
        const Entry* found = nullptr;
        for (const auto& entry : entries_)
            if (entry.context == context && entry.spans.depth_address == p.guest_address) {
                if (found) return 88;
                found = &entry;
            }
        if (!found || found->layout.width != p.width || found->layout.height != p.height ||
            found->layout.pitch != p.pitch || found->layout.samples != p.samples || found->layout.z_format != 3 ||
            found->layout.tile_mode != p.tile_mode || extent.pitch != p.pitch)
            return 88;
        CtrGraphicsImageExtentV1 expected{};
        const int decoded = ctr_video_depth_ms2_extent_v1(&found->layout, &expected);
        if (decoded) return decoded;
        if (expected.bytes != extent.bytes || expected.padded_height != extent.padded_height) return 88;
        return resolve_sampled_interval(context, p.guest_address, extent.bytes, generation, lease, owner);
    }
    // Exact attachment reuse preserves original GPU contents across submissions.
    int resolve_attachment(void* context, const CtrDepthMS2LayoutV1& layout, uint64_t stencil, uint64_t& lease) const {
        const Entry* found = nullptr;
        for (const auto& e : entries_)
            if (e.context == context && e.layout.guest_address == layout.guest_address) {
                if (found) return 88;
                found = &e;
            }
        if (!found) return 62;
        const auto& p = found->layout;
        if (p.width != layout.width || p.height != layout.height || p.pitch != layout.pitch ||
            p.samples != layout.samples || p.z_format != layout.z_format || p.stencil_format != layout.stencil_format ||
            p.tile_mode != layout.tile_mode || p.slice_tile_max != layout.slice_tile_max ||
            p.slice_start != layout.slice_start || p.slice_max != layout.slice_max || p.alt_tile != layout.alt_tile ||
            p.bank_swizzle != layout.bank_swizzle || found->spans.stencil_address != stencil)
            return 88;
        const int status = covers(context, found->token, found->writer);
        if (status) return status;
        lease = found->lease;
        return 0;
    }
    // Original pre-guard CPU bytes, usable only for unaffected backing/padding.
    // GPU logical samples still require real extraction/conversion and a fence.
    int prior_backing(void* context, uint64_t lease, uint64_t generation,
                      std::shared_ptr<const std::vector<unsigned char>>& output) const {
        for (const auto& entry : entries_)
            if (entry.context == context && entry.lease == lease) {
                if (entry.generation != generation || !entry.prior || entry.prior->size() != entry.guard_bytes)
                    return 88;
                const int status = covers(context, entry.token, entry.writer);
                if (status) return status;
                output = entry.prior;
                return 0;
            }
        return 88;
    }
    // Read-only diagnostic: complete original layout for actual overlapping retained guards.
    template <class Observer>
    void observe_overlap(void* context, uint64_t address, uint64_t bytes, Observer&& observer) const {
        if (!bytes || address > UINT64_MAX - bytes) return;
        for (const auto& entry : entries_)
            if (entry.context == context && address < entry.guard_begin + entry.guard_bytes &&
                entry.guard_begin < address + bytes)
                observer(entry.token, entry.lease, entry.writer, entry.generation, entry.layout, entry.spans,
                         entry.guard_begin, entry.guard_bytes, covers(context, entry.token, entry.writer),
                         entry.prior ? entry.prior->size() : 0);
    }
    template <class Observer> void observe(void* context, Observer&& observer) const {
        for (const auto& entry : entries_)
            if (entry.context == context)
                observer(entry.token, entry.lease, entry.writer, entry.generation, entry.spans, entry.guard_begin,
                         entry.guard_bytes);
    }
};
inline DepthResidency runtime_depth_residency;

}
