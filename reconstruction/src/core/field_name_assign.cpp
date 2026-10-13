// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/field_name_assign.h"
#include <atomic>
#include <cstring>
#include <stdexcept>
namespace ctr {
void field_name_assign(std::span<std::byte,36> field,std::uintptr_t identity,
 std::uintptr_t* incoming,std::span<FieldNameEntry> entries,std::uintptr_t context,
 const FieldNameRelease& release){
 if(!incoming)throw std::runtime_error("Missing incoming name slot");
 std::uint32_t flags{};std::memcpy(&flags,field.data()+32,4);
 flags=*incoming?flags|0x100000:flags&0xffefffff;
 std::memcpy(field.data()+32,&flags,4);
 for(auto& entry:entries)if(entry.field==identity){
  if(incoming==&entry.name)return;
  if(entry.name){if(!release)throw std::runtime_error("Missing original name release");release(context,entry.name-16);}
  entry.name=0;
  if(*incoming){auto* refs=reinterpret_cast<std::uint32_t*>(*incoming-8);std::atomic_ref(*refs).fetch_add(1,std::memory_order_seq_cst);entry.name=*incoming;}
  return;
 }
 throw std::runtime_error("Missing original name registry insertion/bootstrap");
}
}
