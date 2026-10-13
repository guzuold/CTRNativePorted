// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/reference_append.h"
#include <atomic>
#include <bit>
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace ctr {
void reference_append(std::span<std::byte,24> header,std::span<const std::uintptr_t> references){
 std::uint32_t old{};std::uint64_t capacity{};std::uintptr_t backing{};std::memcpy(&old,header.data(),4);std::memcpy(&capacity,header.data()+8,8);std::memcpy(&backing,header.data()+16,8);
 const auto added=static_cast<std::uint32_t>(references.size()),total=old+added;
 const auto signed_total=std::bit_cast<std::int32_t>(total);
 const auto slots=backing?static_cast<std::uint32_t>(capacity>>3):0u;
 if(signed_total>std::bit_cast<std::int32_t>(slots)){
  const auto needed=total*8u,available=backing?static_cast<std::uint32_t>(capacity):0u;
  if(std::bit_cast<std::int32_t>(needed)>std::bit_cast<std::int32_t>(available))throw std::runtime_error("Missing original growth014f4d80");
 }
 if(!references.empty()){
  if(!backing||std::bit_cast<std::int32_t>(old)<0)throw std::runtime_error("Missing invalid backing/count domain");
  auto*out=reinterpret_cast<std::byte*>(backing)+static_cast<std::size_t>(old)*8;
  for(std::size_t i=0;i<references.size();++i){auto pointer=references[i];std::memcpy(out+i*8,&pointer,8);if(pointer){auto&counter=*reinterpret_cast<std::uint32_t*>(pointer+8);std::atomic_ref<std::uint32_t>(counter).fetch_add(1,std::memory_order_seq_cst);}}
 }
 std::memcpy(header.data(),&total,4);
}
}
