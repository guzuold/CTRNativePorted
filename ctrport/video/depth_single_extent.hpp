// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_companion.h"
namespace CtrPm4 {
// Exact reached5851 metadata. Public regs_depth GetDepthSliceSize and ImageInfo
// stencil_size. Intervals identify backing only, never pixels, HTILE or page padding.
inline int DepthSingleSpans(const CtrDepthSingleLayoutV1* p, uint64_t stencil, CtrDepthBackingSpansV1* output) {
    if (!p || !output || p->version != 1 || p->size != sizeof(*p)) return 60;
    // Original5098/owner38: complete decoded retail layout in d16_tall_retention_layout.json.
    if (p->width == 2048 && p->height == 8192 && p->pitch == 2048 && p->samples == 1 && p->z_format == 1 &&
        !p->stencil_format && !p->tile_mode && p->slice_tile_max == 262143 && !p->slice_start && !p->slice_max &&
        !p->alt_tile && !p->bank_swizzle) {
        constexpr uint64_t bytes = 33554432;
        if (stencil) return 88;
        if (!p->guest_address || p->guest_address % 256 || p->guest_address > UINT64_MAX - bytes) return 60;
        const CtrDepthBackingSpansV1 prepared{1, sizeof(prepared), p->guest_address, bytes, 0, 0};
        *output = prepared;
        return 0;
    }
    if (p->width != 960 || p->height != 540 || p->pitch != 1024 || p->samples != 1 || p->z_format != 3 ||
        p->stencil_format != 1 || p->tile_mode != 0 || p->slice_tile_max != 10239 || p->slice_start || p->slice_max ||
        p->alt_tile || p->bank_swizzle)
        return 88;
    constexpr uint64_t depthBytes = 2621440, stencilBytes = 552960;
    if (!p->guest_address || p->guest_address % 256 || p->guest_address > UINT64_MAX - depthBytes || !stencil ||
        stencil % 256 || stencil > UINT64_MAX - stencilBytes)
        return 60;
    if (stencil < p->guest_address + depthBytes && p->guest_address < stencil + stencilBytes) return 88;
    const CtrDepthBackingSpansV1 prepared{1, sizeof(prepared), p->guest_address, depthBytes, stencil, stencilBytes};
    *output = prepared;
    return 0;
}
}
