// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/heap_allocate.h"
#include "ctr/core/heap_block_take.h"
#include <bit>
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uintptr_t heap_allocate(std::span<std::byte> memory,std::uintptr_t identity,std::size_t heap_offset,std::uint64_t requested){
 auto check=[&](std::size_t offset,std::size_t size){if(offset>memory.size()||size>memory.size()-offset)throw std::runtime_error("Missing original heap extent");};
 auto q=[&](std::size_t offset){check(offset,8);std::uint64_t v{};std::memcpy(&v,memory.data()+offset,8);return v;};
 auto i=[&](std::size_t offset){check(offset,4);std::uint32_t v{};std::memcpy(&v,memory.data()+offset,4);return v;};
 auto sq=[&](std::size_t offset,std::uint64_t v){check(offset,8);std::memcpy(memory.data()+offset,&v,8);};
 auto si=[&](std::size_t offset,std::uint32_t v){check(offset,4);std::memcpy(memory.data()+offset,&v,4);};
 auto offset=[&](std::uintptr_t pointer){if(pointer<identity||pointer-identity>=memory.size())throw std::runtime_error("Missing original heap pointer view");return static_cast<std::size_t>(pointer-identity);};
 if(requested==0||requested>0xffffffff)return 0;
 const auto size=((requested+7)&~std::uint64_t{7})<24?24:((requested+7)&~std::uint64_t{7});
 unsigned group=0,index=static_cast<unsigned>(size/8);
 if(size>=256){auto bit=63u-static_cast<unsigned>(std::countl_zero(size));auto rounded=size+(std::uint64_t{1}<<(bit-5))-1;bit=63u-static_cast<unsigned>(std::countl_zero(rounded));group=bit-7;index=static_cast<unsigned>((rounded>>(bit-5))^32);}
 if(group>=32||index>=32)throw std::runtime_error("Missing heap bin domain");
 auto fine=i(heap_offset+0x24+group*4)&(0xffffffffu<<index);
 if(!fine){++group;if(group>=32)throw std::runtime_error("Missing wrapped bin search domain");auto coarse=i(heap_offset+0x20)&(0xffffffffu<<group);if(!coarse)return 0;group=std::countr_zero(coarse);fine=i(heap_offset+0x24+group*4);}
 if(!fine)throw std::runtime_error("Missing nonempty bin bitmap");index=std::countr_zero(fine);
 auto bin=heap_offset+group*256+index*8+0x88;auto block=q(bin);if(!block)return 0;
 auto block_offset=offset(block);auto next=q(block_offset+16),previous=q(block_offset+24);sq(offset(next)+24,previous);sq(offset(previous)+16,next);
 const auto heap_identity=identity+heap_offset;
 if(q(bin)==block){sq(bin,next);if(next==heap_identity){auto bits=i(heap_offset+0x24+group*4)&~(std::uint32_t{1}<<index);si(heap_offset+0x24+group*4,bits);if(!bits)si(heap_offset+0x20,i(heap_offset+0x20)&~(std::uint32_t{1}<<group));}}
 auto block_size=q(block_offset+8)&~std::uint64_t{3};std::span<std::byte> predecessor;std::uintptr_t head=0;
 if(block_size>=size+32){auto rem=block_size-size-8;unsigned g=0,j=static_cast<unsigned>(rem/8);if(rem>255){auto bit=63u-static_cast<unsigned>(std::countl_zero(rem));g=bit-7;j=static_cast<unsigned>((rem>>(bit-5))^32);}head=q(heap_offset+g*256+j*8+0x88);auto start=offset(head);check(start,32);predecessor=memory.subspan(start,32);}
 return heap_block_take(memory.subspan(block_offset),block,size,memory.subspan(heap_offset),heap_identity,predecessor,head);
}
}

