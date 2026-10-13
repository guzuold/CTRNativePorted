// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "graphics_companion.h"
#include <array>
#include <cstdint>
namespace CtrPm4 {
struct Bc1MipLayout {uint32_t pitch,height;uint64_t slice_bytes,offset;};
struct Bc1Layout {std::array<Bc1MipLayout,8> mips{};uint32_t levels=0,layers=0,visible_layers=0;uint64_t bytes=0;};
// Exact6773 metadata. Pinned public ImageInfo::UpdateSize: micro-tiled 64-bit
// blocks, mip-major contiguous layers, pow2 padded cube backing (8), view faces6.
inline int Bc1LayoutFor(const CtrGraphicsImageParametersV1& p,Bc1Layout& output){
 const bool flat=p.type==9 && p.width==256 && p.height==256 && p.pitch==256 && p.layers==1 && p.levels==5 && !p.last_array;
 const bool cube=p.type==11 && p.width==128 && p.height==128 && p.pitch==128 && p.layers==8 && p.levels==8 && p.last_array==5;
 if((!flat && !cube) || p.depth!=1 || p.samples!=1 || p.data_format!=35 || p.number_format || p.tile_mode!=13 || p.pow2pad!=1 || p.base_level || p.base_array || p.compression || p.alt_tile || p.swizzle[0]!=4 || p.swizzle[1]!=5 || p.swizzle[2]!=6 || p.swizzle[3]!=7)return 88;
 if(!p.guest_address)return 60;
 Bc1Layout prepared{};prepared.levels=p.levels;prepared.layers=p.layers;prepared.visible_layers=cube?6:1;
 for(uint32_t mip=0;mip<p.levels;++mip){
   uint32_t w=((p.pitch>>mip)+3)/4,h=((p.height>>mip)+3)/4;
   if(!w)w=1;if(!h)h=1;w=(w+7)&~7u;h=(h+7)&~7u;
   const uint64_t slice=uint64_t(w)*h*8;
   prepared.mips[mip]={w,h,slice,prepared.bytes};prepared.bytes+=slice*p.layers;
 }
 if(p.guest_address>UINT64_MAX-prepared.bytes)return 88;
 output=prepared;return 0;
}
}
