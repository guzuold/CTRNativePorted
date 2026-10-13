// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/heap_region_publish.h"
#include <bit>
#include <cstring>
#include <stdexcept>
namespace ctr {
void heap_region_publish(std::span<std::byte> m,std::uintptr_t id,std::size_t h,std::uint64_t length){
 auto check=[&](std::size_t o,std::size_t n){if(o>m.size()||n>m.size()-o)throw std::runtime_error("Missing bounded region/heap");};
 if(((id+h)&7)!=0||h<8)throw std::runtime_error("Missing original alignment diagnostic");
 const auto size=(length-16)&~std::uint64_t{7};
 if(size-24>=0xffffffe9ULL)throw std::runtime_error("Missing original size diagnostic");
 unsigned group=0,index=static_cast<unsigned>(size/8);
 if(size>255){const unsigned bit=63U-static_cast<unsigned>(std::countl_zero(size));group=bit-7;index=static_cast<unsigned>(size>>(bit-5))^32U;}
 const std::size_t bin=0x88+group*256+index*8;check(bin,8);check(0x24+group*4,4);check(h-8,static_cast<std::size_t>(size)+24);
 std::uint64_t head{};std::memcpy(&head,m.data()+bin,8);
 if(head<id||head-id>m.size())throw std::runtime_error("Missing original bin head");
 const auto prior=static_cast<std::size_t>(head-id);check(prior,32);
 auto q=[&](std::size_t o,std::uint64_t v){std::memcpy(m.data()+o,&v,8);};
 auto bits=[&](std::size_t o,std::uint32_t mask){std::uint32_t v;std::memcpy(&v,m.data()+o,4);v|=mask;std::memcpy(m.data()+o,&v,4);};
 const auto block=id+h-8;
 q(h,size|1);q(h+8,head);q(h+16,id);q(prior+24,block);q(bin,block);
 bits(0x20,1U<<group);bits(0x24+group*4,1U<<index);
 q(h+static_cast<std::size_t>(size),block);q(h+static_cast<std::size_t>(size)+8,2);
}
}
