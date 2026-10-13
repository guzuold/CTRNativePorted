// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/allocation_dispatch_fresh.h"
#include <bit>
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uintptr_t allocation_dispatch_fresh(std::span<std::byte> m,std::uintptr_t id,std::size_t p,std::uintptr_t old,std::uint64_t size,std::uint32_t alignment,std::uint32_t mode,std::uint64_t extra,std::uintptr_t known_e0,std::uintptr_t known_130,const PoolFreshOriginal& original,std::uintptr_t known_140){
 auto check=[&](std::size_t o,std::size_t n){if(o>m.size()||n>m.size()-o)throw std::runtime_error("Missing dispatch extent");};
 auto q=[&](std::size_t o){check(o,8);std::uintptr_t v;std::memcpy(&v,m.data()+o,8);return v;};
 auto i=[&](std::size_t o){check(o,4);std::uint32_t v;std::memcpy(&v,m.data()+o,4);return v;};
 auto byte=[&](std::size_t o){check(o,1);return m[o]!=std::byte{};};
 if(!byte(p+0x18)||(!old&&!size))return 0;
 if(old||mode>1||extra||!std::has_single_bit(alignment)||byte(p+0x38)||byte(p+0x4d)||byte(p+0x4b)||byte(p+0x4c))throw std::runtime_error("Missing active dispatch branch");
 auto vt=q(p);if(vt<id||vt-id>=m.size())throw std::runtime_error("Missing dispatch virtual view");auto vo=static_cast<std::size_t>(vt-id);
 if(!known_e0||q(vo+0xe0)!=known_e0||!original||(mode==1?(!known_130||q(vo+0x130)!=known_130):(!known_140||q(vo+0x140)!=known_140)))throw std::runtime_error("Missing original pool dispatch");
 auto count=i(p+0x80)+1;std::memcpy(m.data()+p+0x80,&count,4);
 auto result=original(id+p,0,size,std::max(i(p+0x30),alignment));
 if(!result)throw std::runtime_error("Missing original failure reporting path");
 return result;
}
}
