// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_registration_append.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void metadata_registration_append(std::span<std::byte> m,std::uintptr_t id,std::size_t list,std::uintptr_t value,std::size_t manager,std::int32_t selector,const PoolFreshOriginal& allocate){
 auto check=[&](std::size_t o,std::size_t n){if(o>m.size()||n>m.size()-o)throw std::runtime_error("Missing registration list extent");};
 check(list,16);std::uint64_t data;std::int32_t count,capacity;std::memcpy(&data,m.data()+list,8);std::memcpy(&count,m.data()+list+8,4);std::memcpy(&capacity,m.data()+list+12,4);
 if(count==capacity){
  if(count||data||!manager||!allocate)throw std::runtime_error("Missing original list resize/context");
  check(manager+0x6c,1);if(m[manager+0x6c]!=std::byte{})throw std::runtime_error("Missing enabled context selection");
  auto slot=manager+(selector==1?0x10:selector==2?0x20:selector==4?0x28:0x18);check(slot,8);std::uintptr_t pool;std::memcpy(&pool,m.data()+slot,8);if(pool<id||pool-id>=m.size())throw std::runtime_error("Missing original selected pool");
  auto po=static_cast<std::size_t>(pool-id);check(po+0x30,4);std::uint32_t alignment;std::memcpy(&alignment,m.data()+po+0x30,4);capacity=4;std::memcpy(m.data()+list+12,&capacity,4);
  data=allocation_dispatch_fresh(m,id,po,0,32,alignment,1,0,0x4013d7fe0,0x4013db3c0,allocate);std::memcpy(m.data()+list,&data,8);
 }
 if(count<0||count>capacity||data<id||data-id>m.size())throw std::runtime_error("Missing bounded registration list");
 auto offset=static_cast<std::size_t>(data-id)+static_cast<std::size_t>(count)*8;check(offset,8);++count;std::memcpy(m.data()+list+8,&count,4);std::memcpy(m.data()+offset,&value,8);
}
}
