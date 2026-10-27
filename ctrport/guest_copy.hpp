// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "flat_loader.hpp"
#include <mutex>
#include <limits>
#include <algorithm>
namespace ctrport {
// Owner must serialize all unmap/reprotect and guest writes with the capture
// boundary. This gives lifetime/permission checks, not a coherent GPU snapshot.
// Revocation waits for copies; shared ownership prevents allocation reuse.
class GuestCopyRegion {
 std::shared_ptr<void> owner;
 std::uintptr_t base;std::size_t extent;
 std::vector<std::pair<std::uint64_t,std::uint64_t>> backed;
 std::mutex mutex;bool active=true;
 void check(std::uintptr_t address,std::size_t size,bool writing){
  if(!active)throw std::runtime_error("Guest mapping expired");
  if(address<base || address-base>extent || size>extent-(address-base))throw std::runtime_error("Guest copy range outside owner");
  if(size>(std::numeric_limits<std::uintptr_t>::max)()-address)throw std::runtime_error("Guest copy overflow");
  if(size){
   auto offset=static_cast<std::uint64_t>(address-base);const auto finish=offset+size;
   for(const auto& span:backed){if(span.first>offset)break;if(span.second>offset)offset=span.second;if(offset>=finish)break;}
   if(offset<finish)throw std::runtime_error("Guest copy unbacked image span");
  }
  auto current=address;const auto end=address+size;
  while(current<end){
   MEMORY_BASIC_INFORMATION info{};
   if(!VirtualQuery(reinterpret_cast<void*>(current),&info,sizeof(info)) || info.State!=MEM_COMMIT || info.AllocationBase!=owner.get() || (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))throw std::runtime_error("Guest copy unmapped or foreign allocation");
   const auto protection=info.Protect&0xff;
   const bool writable=protection==PAGE_READWRITE || protection==PAGE_WRITECOPY || protection==PAGE_EXECUTE_READWRITE || protection==PAGE_EXECUTE_WRITECOPY;
   const bool readable=writable || protection==PAGE_READONLY || protection==PAGE_EXECUTE_READ;
   if(!readable || (writing && !writable))throw std::runtime_error("Guest copy permissions rejected");
   const auto start=reinterpret_cast<std::uintptr_t>(info.BaseAddress);
   if(info.RegionSize>(std::numeric_limits<std::uintptr_t>::max)()-start || start+info.RegionSize<=current)throw std::runtime_error("Guest copy page extent overflow");
   current=(std::min)(end,start+info.RegionSize);
  }
 }
public:
 GuestCopyRegion(std::shared_ptr<void> allocation,std::size_t size,std::vector<std::pair<std::uint64_t,std::uint64_t>> spans={}):owner(std::move(allocation)),base(reinterpret_cast<std::uintptr_t>(owner.get())),extent(size),backed(std::move(spans)){
  if(!owner || !size || size>(std::numeric_limits<std::uintptr_t>::max)()-base)throw std::runtime_error("Guest copy ownership invalid");
  if(backed.empty())backed.push_back({0,size});
  std::sort(backed.begin(),backed.end());
  for(const auto& span:backed)if(span.first>=span.second || span.second>extent)throw std::runtime_error("Guest copy backed extent invalid");
 }
 void revoke(){std::lock_guard<std::mutex> guard(mutex);active=false;}
 void read(std::uintptr_t address,void* output,std::size_t size){
  if(size>16*1024*1024 || (!output && size))throw std::runtime_error("Guest snapshot bound rejected");
  std::lock_guard<std::mutex> guard(mutex);check(address,size,false);
  // Stage before writing trusted output, so failed guest reads leave it unchanged.
  std::vector<unsigned char> snapshot(size);SIZE_T copied=0;
  if(size && (!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),snapshot.data(),size,&copied) || copied!=size))throw std::runtime_error("Guest snapshot read failed");
  if(size)std::memcpy(output,snapshot.data(),size);
 }
 void write(std::uintptr_t address,const void* input,std::size_t size){
  if(size>16*1024*1024 || (!input && size))throw std::runtime_error("Guest output bound rejected");
  std::lock_guard<std::mutex> guard(mutex);check(address,size,true);
  // Complete preflight before any guest write. Host input must be owned/stable.
  if(size)std::memcpy(reinterpret_cast<void*>(address),input,size);
 }
};
}
