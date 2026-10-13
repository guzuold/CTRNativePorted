// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "guest_copy_api.h"
#include "kernel_time.hpp"
#include <cstring>
#include "guest_stack.hpp"
namespace ctrport {
// Synchronous CPU copy only. Published runtime-owned RW extents (direct mappings
// and guest stacks). Revocation uses this same mutex before native allocation free.
// No GPU-coherent snapshot or arbitrary VirtualQuery ownership inference.
inline int owned_guest_copy(void* context,uint64_t address,void* buffer,uint64_t bytes,bool write) noexcept {
 if(!context || !address || !buffer || !bytes || bytes>16*1024*1024 || address>UINT64_MAX-bytes)return 1;
 const auto host=reinterpret_cast<uintptr_t>(buffer);
 if(host>UINTPTR_MAX-bytes || (host<address+bytes && host+bytes>address))return 1;
 std::lock_guard<std::mutex> guard(owned_output_lock);
 bool owned=false;
 for(const auto& item:owned_output_extents)for(const auto& extent:item.second)
  if(address>=extent.first && address+bytes<=extent.second)owned=true;
 if(!owned)return 1;
 uint64_t cursor=address;
 while(cursor<address+bytes){
  MEMORY_BASIC_INFORMATION info{};
  if(!VirtualQuery(reinterpret_cast<void*>(cursor),&info,sizeof(info)) || info.State!=MEM_COMMIT || info.Protect!=PAGE_READWRITE)return 1;
  const auto end=reinterpret_cast<uint64_t>(info.BaseAddress)+info.RegionSize;
  if(end<=cursor)return 1;cursor=end;
 }
 if(write)std::memcpy(reinterpret_cast<void*>(address),buffer,static_cast<size_t>(bytes));
 else std::memcpy(buffer,reinterpret_cast<void*>(address),static_cast<size_t>(bytes));
 return 0;
}
inline int owned_guest_read(void* c,uint64_t a,void* b,uint64_t n) noexcept{return owned_guest_copy(c,a,b,n,false);}
inline int owned_guest_write(void* c,uint64_t a,const void* b,uint64_t n) noexcept{return owned_guest_copy(c,a,const_cast<void*>(b),n,true);}
inline int owned_guest_copy_context=1;
inline CtrGuestCopyApi make_owned_guest_copy_api(){return {sizeof(CtrGuestCopyApi),1,&owned_guest_copy_context,&owned_guest_read,&owned_guest_write};}
inline int owned_guest_copy_control(){
 const auto api=make_owned_guest_copy_api();uint64_t address=0;
 unsigned char input[48],output[48];std::memset(input,0x63,48);std::memset(output,0x79,48);
 unsigned rejected=0;
 {GuestStack stack(32768,4096);address=reinterpret_cast<uint64_t>(stack.bottom());
  if(api.write(api.context,address,input,48) || api.read(api.context,address,output,48) || std::memcmp(input,output,48))return 1;
  for(auto location:{address-32,reinterpret_cast<uint64_t>(stack.top())-32,UINT64_MAX-31})if(api.write(api.context,location,input,48))++rejected;
  if(api.write(api.context,address,stack.bottom(),48))++rejected;
  DWORD old=0;if(!VirtualProtect(stack.bottom(),4096,PAGE_READONLY,&old))return 1;
  if(api.write(api.context,address,input,48))++rejected;
  if(!VirtualProtect(stack.bottom(),4096,old,&old))return 1;
 }
 std::memset(output,0x79,48);if(api.read(api.context,address,output,48))++rejected;
 for(auto v:output)if(v!=0x79)return 1;
 if(rejected!=6)return 1;
 std::cout<<"{\"scope\":\"owned_guest_stack_copy_control\",\"bytes\":48,\"negative_rejections\":6,\"revoked_read_untouched\":true,\"GPU_coherence\":false}\n";return 0;
}

}
