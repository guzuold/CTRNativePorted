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
inline int guest_copy_api_control(){
 auto memory=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
 if(!memory)return 1;
 auto owner=std::shared_ptr<void>(memory,[](void* p){VirtualFree(p,0,MEM_RELEASE);});
 GuestCopyRegion region(owner,4096);const auto api=make_guest_copy_api(region);
 const auto address=reinterpret_cast<std::uintptr_t>(memory);unsigned char input[64],output[64];
 std::memset(input,0x62,64);std::memset(output,0x71,64);
 if(api.size!=sizeof(api) || api.version!=1 || api.write(api.context,address,input,64) || api.read(api.context,address,output,64) || std::memcmp(input,output,64))return 1;
 unsigned rejected=0;
 if(api.read(nullptr,address,output,40)==1)++rejected;
 if(api.read(api.context,address,nullptr,40)==1)++rejected;
 if(api.write(api.context,address,input,16ULL*1024*1024+1)==1)++rejected;
 DWORD previous=0;if(!VirtualProtect(memory,4096,PAGE_READONLY,&previous))return 1;
 if(api.write(api.context,address,input,40)==1)++rejected;
 region.revoke();std::memset(output,0x71,64);
 if(api.read(api.context,address,output,64)==1)++rejected;
 for(auto byte:output)if(byte!=0x71)return 1;
 if(rejected!=5)return 1;
 std::cout<<"{\"scope\":\"guest_copy_C_ABI_control\",\"actual_bytes\":64,\"negative_rejections\":5,\"exceptions_cross_ABI\":false,\"retail_integrated\":false}\n";return 0;
}
}
