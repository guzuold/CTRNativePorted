// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/heap_allocate_aligned.h"
#include "ctr/core/heap_block_take.h"
#include <bit>
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uintptr_t heap_allocate_aligned(std::span<std::byte> memory,std::uintptr_t identity,std::size_t heap_offset,std::uint64_t alignment,std::uint64_t requested){
 auto check=[&](std::size_t offset,std::size_t size){if(offset>memory.size()||size>memory.size()-offset)throw std::runtime_error("Missing original heap extent");};
 auto q=[&](std::size_t offset){check(offset,8);std::uint64_t v{};std::memcpy(&v,memory.data()+offset,8);return v;};
 auto i=[&](std::size_t offset){check(offset,4);std::uint32_t v{};std::memcpy(&v,memory.data()+offset,4);return v;};
 auto sq=[&](std::size_t offset,std::uint64_t v){check(offset,8);std::memcpy(memory.data()+offset,&v,8);};
 auto si=[&](std::size_t offset,std::uint32_t v){check(offset,4);std::memcpy(memory.data()+offset,&v,4);};
 auto offset=[&](std::uintptr_t pointer){if(pointer<identity||pointer-identity>=memory.size())throw std::runtime_error("Missing original heap pointer view");return static_cast<std::size_t>(pointer-identity);};
 if(alignment<4||!std::has_single_bit(alignment))throw std::runtime_error("Missing unsupported alignment domain");
 if(requested==0||requested>0xffffffff)return 0;
 const auto size=((requested+7)&~std::uint64_t{7})<24?24:((requested+7)&~std::uint64_t{7});
 const auto extra=size+alignment+31;
 if(extra>0xfffffffe)return 0;
 const auto search=alignment<=8?size:((extra+alignment)&(0-alignment));
 unsigned group=0,index=static_cast<unsigned>(search/8);
 if(search>=256){auto bit=63u-static_cast<unsigned>(std::countl_zero(search));auto rounded=search+(std::uint64_t{1}<<(bit-5))-1;bit=63u-static_cast<unsigned>(std::countl_zero(rounded));group=bit-7;index=static_cast<unsigned>((rounded>>(bit-5))^32);}
 if(group>=32||index>=32)throw std::runtime_error("Missing heap bin domain");
 auto fine=i(heap_offset+0x24+group*4)&(0xffffffffu<<index);
 if(!fine){++group;if(group>=32)throw std::runtime_error("Missing wrapped bin search domain");auto coarse=i(heap_offset+0x20)&(0xffffffffu<<group);if(!coarse)return 0;group=std::countr_zero(coarse);fine=i(heap_offset+0x24+group*4);}
 if(!fine)throw std::runtime_error("Missing nonempty bin bitmap");index=std::countr_zero(fine);
 auto bin=heap_offset+group*256+index*8+0x88;auto block=q(bin);if(!block)return 0;
 auto block_offset=offset(block);auto next=q(block_offset+16),previous=q(block_offset+24);sq(offset(next)+24,previous);sq(offset(previous)+16,next);
 const auto heap_identity=identity+heap_offset;
 if(q(bin)==block){sq(bin,next);if(next==heap_identity){auto bits=i(heap_offset+0x24+group*4)&~(std::uint32_t{1}<<index);si(heap_offset+0x24+group*4,bits);if(!bits)si(heap_offset+0x20,i(heap_offset+0x20)&~(std::uint32_t{1}<<group));}}
 auto payload=block+16;auto aligned=(payload+alignment-1)&(0-alignment);auto prefix=aligned-payload;
 if(prefix&&prefix<32){auto gap=32-prefix;if(gap<=alignment)gap=alignment;aligned=(aligned+alignment-1+gap)&(0-alignment);prefix=aligned-payload;}
 auto original_size=q(block_offset+8)&~std::uint64_t{3};
 if(prefix&&original_size>=prefix+32){
  auto new_block=block+prefix;auto nb=block_offset+static_cast<std::size_t>(prefix);
  sq(nb+8,(q(nb+8)&3)|(original_size-prefix));sq(block_offset+8,(q(block_offset+8)&3)|(prefix-8));
  auto remaining_footer=(q(nb+8)-8)&~std::uint64_t{3};sq(nb+16+static_cast<std::size_t>(remaining_footer),new_block);
  check(nb+24+static_cast<std::size_t>(remaining_footer),1);memory[nb+24+static_cast<std::size_t>(remaining_footer)]|=std::byte{2};
  sq(nb+8,q(nb+8)|3);auto flags=q(block_offset+8);auto prefix_size=flags&~std::uint64_t{3};
  sq(block_offset+16+static_cast<std::size_t>((flags-8)&~std::uint64_t{3}),block);
  unsigned g=0,j=static_cast<unsigned>(prefix_size/8);if(prefix_size>255){auto bit=63u-static_cast<unsigned>(std::countl_zero(prefix_size));g=bit-7;j=static_cast<unsigned>((prefix_size>>(bit-5))^32);}
  auto prefix_bin=heap_offset+g*256+j*8+0x88;auto prior=q(prefix_bin);
  sq(block_offset+16,prior);sq(block_offset+24,heap_identity);sq(offset(prior)+24,block);sq(prefix_bin,block);
  si(heap_offset+0x20,i(heap_offset+0x20)|(1U<<g));si(heap_offset+0x24+g*4,i(heap_offset+0x24+g*4)|(1U<<j));
  block=new_block;block_offset=nb;
 }
 auto block_size=q(block_offset+8)&~std::uint64_t{3};std::span<std::byte> predecessor;std::uintptr_t head=0;
 if(block_size>=size+32){auto rem=block_size-size-8;unsigned g=0,j=static_cast<unsigned>(rem/8);if(rem>255){auto bit=63u-static_cast<unsigned>(std::countl_zero(rem));g=bit-7;j=static_cast<unsigned>((rem>>(bit-5))^32);}head=q(heap_offset+g*256+j*8+0x88);auto start=offset(head);check(start,32);predecessor=memory.subspan(start,32);}
 return heap_block_take(memory.subspan(block_offset),block,size,memory.subspan(heap_offset),heap_identity,predecessor,head);
}
}

