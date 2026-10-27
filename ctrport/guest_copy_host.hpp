// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "guest_copy.hpp"
#include "guest_copy_api.h"
namespace ctrport {
inline int guest_copy_read(void* context,std::uint64_t address,void* output,std::uint64_t size) noexcept {
 if(!context || size>16*1024*1024)return 1;
 try{static_cast<GuestCopyRegion*>(context)->read(static_cast<std::uintptr_t>(address),output,static_cast<std::size_t>(size));return 0;}catch(...){return 1;}
}
inline int guest_copy_write(void* context,std::uint64_t address,const void* input,std::uint64_t size) noexcept {
 if(!context || size>16*1024*1024)return 1;
 try{static_cast<GuestCopyRegion*>(context)->write(static_cast<std::uintptr_t>(address),input,static_cast<std::size_t>(size));return 0;}catch(...){return 1;}
}
inline CtrGuestCopyApi make_guest_copy_api(GuestCopyRegion& region){return {sizeof(CtrGuestCopyApi),1,&region,&guest_copy_read,&guest_copy_write};}
}
