// SPDX-License-Identifier: GPL-2.0-only
// Runtime-owned direct mappings only. Synchronous C ABI; caller retains owner.
#pragma once
#include "direct_memory.hpp"
#include "guest_copy_api.h"
namespace ctrport {
inline int direct_copy_read(void* context,std::uint64_t address,void* output,std::uint64_t size) noexcept {
    if(!context || size>16*1024*1024)return 1;
    try{static_cast<DirectMemoryBacking*>(context)->copy_mapped(address,output,static_cast<std::size_t>(size),false);return 0;}catch(...){return 1;}
}
inline int direct_copy_write(void* context,std::uint64_t address,const void* input,std::uint64_t size) noexcept {
    if(!context || size>16*1024*1024)return 1;
    try{static_cast<DirectMemoryBacking*>(context)->copy_mapped(address,const_cast<void*>(input),static_cast<std::size_t>(size),true);return 0;}catch(...){return 1;}
}
inline CtrGuestCopyApi make_direct_copy_api(DirectMemoryBacking& owner){return {sizeof(CtrGuestCopyApi),1,&owner,direct_copy_read,direct_copy_write};}
inline int direct_copy_control(){
    DirectMemoryBacking owner(65536);std::int64_t physical=-1;void* memory=nullptr;
    if(owner.allocate(0,65536,16384,0,0,&physical) || owner.map(&memory,16384,51,0,physical,0))return 1;
    const auto api=make_direct_copy_api(owner);const auto address=reinterpret_cast<std::uint64_t>(memory);
    unsigned char input[64],output[64];std::memset(input,0x63,64);std::memset(output,0x79,64);
    if(api.write(api.context,address,input,64) || api.read(api.context,address,output,64) || std::memcmp(input,output,64))return 1;
    unsigned rejected=0;
    for(auto location:{address+16384,address+16384-32,UINT64_MAX-31})if(api.write(api.context,location,input,64))++rejected;
    if(api.write(api.context,address,memory,64))++rejected;
    DWORD old=0;if(!VirtualProtect(memory,16384,PAGE_READONLY,&old))return 1;
    if(api.write(api.context,address,input,64))++rejected;
    if(api.read(api.context,address,output,64) || std::memcmp(input,output,64))return 1;
    if(!VirtualProtect(memory,16384,PAGE_READWRITE,&old))return 1;
    std::memset(output,0x79,64);if(api.read(api.context,address+16384,output,64))++rejected;
    for(auto byte:output)if(byte!=0x79)return 1;
    if(rejected!=6)return 1;
    std::cout<<"{\"scope\":\"owned_direct_mapping_copy_C_ABI\",\"actual_bytes\":64,\"negative_rejections\":6,\"failed_read_untouched\":true,\"GPU_coherence\":false}\n";return 0;
}
}
