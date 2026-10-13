// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/pool_aligned_allocate.h"
#include <cstring>
#include <algorithm>
#include <stdexcept>
namespace ctr {
std::uintptr_t pool_aligned_allocate(std::span<std::byte> m,std::uintptr_t id,std::size_t pool,std::uint64_t requested,std::uint32_t alignment,const PoolAlignedOriginal& original){
 auto check=[&](std::size_t o,std::size_t n){if(o>m.size()||n>m.size()-o)throw std::runtime_error("Missing bounded pool memory");};
 auto off=[&](std::uintptr_t p){if(p<id||p-id>=m.size())throw std::runtime_error("Missing pool pointer view");return static_cast<std::size_t>(p-id);};
 auto q=[&](std::size_t o){check(o,8);std::uint64_t v;std::memcpy(&v,m.data()+o,8);return v;};
 auto sq=[&](std::size_t o,std::uint64_t v){check(o,8);std::memcpy(m.data()+o,&v,8);};
 check(pool+0x4b,1);auto adjusted=(requested+(m[pool+0x4b]!=std::byte{}?4:0)+15)&~std::uint64_t{7};
 auto result=original?original(q(pool+0xa8),alignment,adjusted):heap_allocate_aligned(m,id,off(q(pool+0xa8)),alignment,adjusted);if(!result)return 0;
 auto size=q(off(result)-8)&~std::uint64_t{3};sq(off((result+size-8)&~std::uintptr_t{7}),requested);
 auto actual=q(pool+0x70)+size+8;sq(pool+0x70,actual);sq(pool+0x78,std::max(actual,q(pool+0x78)));
 auto logical=q(pool+0x60)+requested;sq(pool+0x60,logical);sq(pool+0x68,std::max(logical,q(pool+0x68)));
 auto count=q(pool+0x50);if(requested){++count;sq(pool+0x50,count);sq(pool+0x58,std::max(count,q(pool+0x58)));}else sq(pool+0x50,count-1);
 return result;
}
}
