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
}
