// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_storage_create.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uintptr_t metadata_storage_create(std::uintptr_t context,bool use_factory,
 std::uintptr_t type_slot,std::uintptr_t type_name,const OriginalIntegerCall2& type_lookup,
 const OriginalIntegerCall3& factory,const OriginalIntegerCall2& allocate,
 const OriginalIntegerCall2& construct,const OriginalIntegerCall2& initialize){
 if(use_factory){
  if(!type_slot||!type_lookup||!factory)throw std::runtime_error("Missing original cached type/factory");
  type_lookup(type_slot,type_name);std::uintptr_t type{};std::memcpy(&type,reinterpret_cast<const void*>(type_slot),8);
  return factory(type,context,0);
 }
 if(!allocate||!construct||!initialize)throw std::runtime_error("Missing original metadata allocation/constructor/virtual40");
 const auto storage=allocate(0x98,context);
 if(!storage)throw std::runtime_error("Missing successful metadata storage allocation");
 construct(storage,0);initialize(storage,0);return storage;
}
}
