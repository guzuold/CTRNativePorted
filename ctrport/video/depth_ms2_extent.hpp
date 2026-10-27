// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_companion.h"
namespace CtrPm4 {
inline int DepthMS2Extent(const CtrDepthMS2LayoutV1* p, CtrGraphicsImageExtentV1* output) {
    if (!p || !output || p->version != 1 || p->size != sizeof(*p)) return 60;
    if (p->width != 1920 || p->height != 1080 || p->pitch != 1920 || p->samples != 2 || p->z_format != 3 ||
        p->stencil_format != 1 || p->tile_mode != 1 || p->slice_tile_max != 34559 || p->slice_start || p->slice_max ||
        p->alt_tile || p->bank_swizzle)
        return 88;
    // Public regs_depth GetDepthSliceSize: (tile_max+1)*64*(32/8)*2.
    constexpr uint64_t bytes = 17694720;
    if (!p->guest_address || p->guest_address % 256 || p->guest_address > UINT64_MAX - bytes) return 60;
    CtrGraphicsImageExtentV1 result{1, sizeof(result), 1920, 1152, p->guest_address, bytes};
    *output = result;
    return 0;
}
inline int DepthMS2Spans(const CtrDepthMS2LayoutV1* p, uint64_t stencil, CtrDepthBackingSpansV1* output) {
    if (!output) return 60;
    CtrGraphicsImageExtentV1 depth{};
    const int status = DepthMS2Extent(p, &depth);
    if (status) return status;
    // Public ImageInfo(DepthBuffer): pitch * logical size.height * sizeof(u8).
    // No MSAA multiplier; this describes backing identity, not export semantics.
    constexpr uint64_t stencilBytes = 1920ULL * 1080;
    if (!stencil || stencil % 256 || stencil > UINT64_MAX - stencilBytes) return 60;
    if (stencil < depth.guest_address + depth.bytes && depth.guest_address < stencil + stencilBytes) return 88;
    CtrDepthBackingSpansV1 result{1, sizeof(result), depth.guest_address, depth.bytes, stencil, stencilBytes};
    *output = result;
    return 0;
}
}
