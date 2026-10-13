// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
// Bounded adaptation of public SearchBinaryInfo/GetParams. Host snapshot only.
// No arbitrary guest pointers; missing bytes/footer never become empty shader.
namespace CtrPm4 {
struct ShaderBinaryMetadata {size_t footer=0;uint32_t codeBytes=0;uint64_t hash=0;};
inline int FindShaderBinaryMetadata(const void* bytes,size_t size,ShaderBinaryMetadata& output) {
    if(!bytes || size<8 || size>16*1024*1024)return 60;
    const auto* data=static_cast<const unsigned char*>(bytes);
    const auto u32=[&](size_t at){uint32_t v;std::memcpy(&v,data+at,4);return v;};
    const auto matches=[&](size_t at){return at<=size && size-at>=28 && !std::memcmp(data+at,"OrbShdr",7);};
    size_t found=SIZE_MAX;
    if(u32(0)==0xbeeb03ffU) {
        const uint64_t at=(uint64_t(u32(4))+1)*8;
        if(at<=SIZE_MAX && matches(static_cast<size_t>(at)))found=static_cast<size_t>(at);
    }
    if(found==SIZE_MAX)for(size_t at=0;at<0x10000 && at<=size && size-at>=28;at+=4)
        if(matches(at)){found=at;break;}
    if(found==SIZE_MAX)return 88;
    ShaderBinaryMetadata prepared;prepared.footer=found;prepared.codeBytes=u32(found+8)>>8;
    if(!prepared.codeBytes || (prepared.codeBytes&3) || prepared.codeBytes>size)return 60;
    std::memcpy(&prepared.hash,data+found+16,8);output=prepared;return 0;
}
}
