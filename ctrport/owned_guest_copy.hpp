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

}
