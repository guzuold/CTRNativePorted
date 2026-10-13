// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/registration_list_create.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uintptr_t registration_list_create(std::span<std::byte> m,std::uintptr_t base,std::size_t manager,std::int32_t selector,const PoolFreshOriginal& allocate,const std::function<std::uintptr_t()>& context_lookup){
 auto check=[&](std::size_t o,std::size_t n){if(o>m.size()||n>m.size()-o)throw std::runtime_error("Missing registration allocation extent");};
 auto readq=[&](std::size_t o){check(o,8);std::uintptr_t v;std::memcpy(&v,m.data()+o,8);return v;};
 auto readi=[&](std::size_t o){check(o,4);std::int32_t v;std::memcpy(&v,m.data()+o,4);return v;};
 auto off=[&](std::uintptr_t p){if(p<base||p-base>=m.size())throw std::runtime_error("Missing context pointer extent");return static_cast<std::size_t>(p-base);};
 check(manager+0x6c,1);std::uintptr_t pool{};
 if(m[manager+0x6c]==std::byte{}){
  auto slot=manager+(selector==1?0x10:selector==2?0x20:selector==4?0x28:0x18);pool=readq(slot);
 }else{
  if(!context_lookup)throw std::runtime_error("Missing original context lookup provider");
  auto context=off(context_lookup());auto index=readi(context+0x18);auto entries=off(readq(off(readq(context+0x10))+0x20));
  if(index<0||index>16||selector<0)throw std::runtime_error("Missing bounded context frame selection");
  bool found=false;
   for(auto i=index;i>=0;--i){auto frame=off(readq(entries+static_cast<std::size_t>(i)*8));auto descriptor=off(readq(frame+0x18));
    if(selector<readi(descriptor+0xc)){auto pools=off(readq(descriptor+0x20));pool=readq(pools+static_cast<std::size_t>(selector)*8);if(pool){found=true;break;}}
   }
  if(!found)throw std::runtime_error("Missing context fallback pool");
 }
 if(pool<base||pool-base>=m.size())throw std::runtime_error("Missing selected allocation pool");
 auto po=static_cast<std::size_t>(pool-base);check(po+0x30,4);std::uint32_t alignment;std::memcpy(&alignment,m.data()+po+0x30,4);
 auto result=allocation_dispatch_fresh(m,base,po,0,16,alignment,1,0,0x4013d7fe0,0x4013db3c0,allocate);
 if(result<base||result-base>m.size())throw std::runtime_error("Missing allocated registration storage");
 auto offset=static_cast<std::size_t>(result-base);check(offset,16);std::memset(m.data()+offset,0,16);return result;
}
}
