// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_companion.h"
namespace CtrPm4 {
inline int DepthD16Extent(const CtrDepthD16LayoutV1* p,CtrGraphicsImageExtentV1* output) {
    if(!p || !output || p->version!=1 || p->size!=sizeof(*p))return 60;
    // Exact reached shape only. Public tile0 D16 macro256x128, split64.
    if(p->width!=1024 || p->height!=1024 || p->pitch!=1024 || p->samples!=1 ||
       p->z_format!=1 || p->stencil_format || p->tile_mode || p->slice_tile_max!=16383 ||
       p->slice_start || p->slice_max || p->alt_tile || p->bank_swizzle)return 88;
    constexpr uint64_t bytes=2097152;
    if(!p->guest_address || p->guest_address%256 || p->guest_address>UINT64_MAX-bytes)return 60;
    CtrGraphicsImageExtentV1 result{1,sizeof(result),1024,1024,p->guest_address,bytes};
    *output=result;return 0;
}
}
