// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <cstddef>
#include <array>
#include "shader_inputs.h"
// Public layout from shadPS4 src/core/libraries/videoout/buffer.h,
// revision e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later.
namespace CtrVideo {
struct DisplayBufferAttributes {
    std::uint32_t format;std::int32_t tiling;std::int32_t aspect;
    std::uint32_t width,height,pitch,option,reserved0;
    std::uint64_t reserved1;
};
static_assert(sizeof(DisplayBufferAttributes)==40);
static_assert(offsetof(DisplayBufferAttributes,reserved1)==32);
struct PreparedDisplayRange {std::uint64_t address,size;};
enum class DisplayInputResult {Prepared,Invalid,MissingTiledLayout,MissingFormat};
// Metadata preparation only. Does not pin backing/register a surface/present.
// Caller supplies an immutable captured attribute value, never a guest pointer.
inline DisplayInputResult PrepareDisplayRange(const DisplayBufferAttributes& a,
    std::uint64_t address,PreparedDisplayRange& output){
    if(!address || a.reserved0 || a.reserved1 || a.aspect || a.option ||
       !a.width || !a.height || a.width>a.pitch || a.tiling<0 || a.tiling>1)
        return DisplayInputResult::Invalid;
    if(a.tiling==0)return DisplayInputResult::MissingTiledLayout;
    std::uint64_t bpp=0;
    switch(a.format){
    case 0x80000000U:case 0x80002200U:case 0x88060000U:
    case 0x88000000U:case 0x88740000U:bpp=4;break;
    case 0xC1060000U:bpp=8;break;
    default:return DisplayInputResult::MissingFormat;
    }
    const std::uint64_t row=std::uint64_t(a.pitch)*bpp;
    if(row>UINT64_MAX/a.height)return DisplayInputResult::Invalid;
    const auto size=row*a.height;
    if(address>UINT64_MAX-size)return DisplayInputResult::Invalid;
    output={address,size};return DisplayInputResult::Prepared;
}
struct PreparedDisplayBatch {
    DisplayBufferAttributes attributes{};
    std::array<PreparedDisplayRange,16> ranges{};
    std::uint32_t first=0,count=0;
};
// Both snapshots must be held live by caller from the same consistent capture.
inline DisplayInputResult PrepareDisplayBatch(const CtrShaderSnapshot& attributes,
    std::uint64_t attribute_address,const CtrShaderSnapshot& addresses,
    std::uint64_t array_address,std::int32_t first,std::int32_t count,
    PreparedDisplayBatch& output){
    if(first<0 || count<=0 || first>=16 || count>16-first)return DisplayInputResult::Invalid;
    PreparedDisplayBatch prepared{};
    std::array<std::uint64_t,16> raw{};
    if(ctr_shader_snapshot_read(&attributes,attribute_address,&prepared.attributes,sizeof(prepared.attributes)) ||
       ctr_shader_snapshot_read(&addresses,array_address,raw.data(),sizeof(raw[0])*count))
        return DisplayInputResult::Invalid;
    for(std::int32_t n=0;n<count;++n){
        const auto result=PrepareDisplayRange(prepared.attributes,raw[n],prepared.ranges[n]);
        if(result!=DisplayInputResult::Prepared)return result;
    }
    prepared.first=static_cast<std::uint32_t>(first);
    prepared.count=static_cast<std::uint32_t>(count);
    output=prepared;return DisplayInputResult::Prepared;
}

}
