// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/heap_block_take.h"
#include <cstring>
#include <bit>
#include <stdexcept>
namespace ctr {
std::uintptr_t heap_block_take(std::span<std::byte> block,std::uintptr_t identity,std::uint64_t requested,std::span<std::byte> heap,std::uintptr_t heap_identity,std::span<std::byte> predecessor,std::uintptr_t predecessor_identity){
 if(!identity)return 0;
 if(block.size()<16)throw std::runtime_error("Missing block extent");
 std::uint64_t flags{};std::memcpy(&flags,block.data()+8,8);auto size=flags&~std::uint64_t{3};
 if(size>=requested+32){
  auto load=[&](std::size_t offset){std::uint64_t v{};if(offset>block.size()||block.size()-offset<8)throw std::runtime_error("Missing split extent");std::memcpy(&v,block.data()+offset,8);return v;};
  auto store=[&](std::size_t offset,std::uint64_t v){if(offset>block.size()||block.size()-offset<8)throw std::runtime_error("Missing split extent");std::memcpy(block.data()+offset,&v,8);};
  const auto remainder=size-requested-8;
  unsigned group=0,index=static_cast<unsigned>(remainder/8);
  if(remainder>255){const auto bit=63u-static_cast<unsigned>(std::countl_zero(remainder));group=bit-7;index=static_cast<unsigned>((remainder>>(bit-5))^32);}
  const auto bin=static_cast<std::size_t>(group)*256+static_cast<std::size_t>(index)*8+0x88;
  if(!heap_identity||group>=32||index>=32||bin>heap.size()||heap.size()-bin<8||0x24+group*4+4>heap.size()||predecessor.size()<32||!predecessor_identity||size+17>block.size())throw std::runtime_error("Missing original bounded bin/predecessor");
  std::uint64_t head{};std::memcpy(&head,heap.data()+bin,8);if(head!=predecessor_identity)throw std::runtime_error("Missing actual bin predecessor view");
  store(requested+16,(load(requested+16)&3)|remainder);store(8,(flags&3)|requested);
  store(size+8,identity+requested+8);block[size+16]|=std::byte{2};
  auto remainder_flags=load(requested+16);store(requested+16,remainder_flags|1);store(requested+8,identity);store(requested+16,remainder_flags|3);
  store(requested+24,head);store(requested+32,heap_identity);
  auto link=identity+requested+8;std::memcpy(predecessor.data()+24,&link,8);std::memcpy(heap.data()+bin,&link,8);
  std::uint32_t bitmap{};std::memcpy(&bitmap,heap.data()+0x20,4);bitmap|=std::uint32_t{1}<<group;std::memcpy(heap.data()+0x20,&bitmap,4);
  std::memcpy(&bitmap,heap.data()+0x24+group*4,4);bitmap|=std::uint32_t{1}<<index;std::memcpy(heap.data()+0x24+group*4,&bitmap,4);
  flags=load(8);
 }
 auto next=((flags-8)&~std::uint64_t{3})+24;
 if(next>=block.size())throw std::runtime_error("Missing successor extent");
 block[next]&=std::byte{0xfd};block[8]&=std::byte{0xfe};return identity+16;
}
}
