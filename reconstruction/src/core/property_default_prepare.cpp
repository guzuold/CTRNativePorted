// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/property_default_prepare.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void property_default_prepare(std::span<const std::byte,80> property,
 const std::function<std::uintptr_t(std::uintptr_t)>& lookup,const PropertyOriginal& original,
 const std::function<void(std::uintptr_t,std::uintptr_t)>& deserialize,
 const std::function<void(std::uintptr_t,std::uintptr_t)>& initialize){
 std::uintptr_t field{},object{};std::memcpy(&field,property.data()+64,8);std::memcpy(&object,property.data()+72,8);
 if(!field||!lookup||!original||!deserialize||!initialize)throw std::runtime_error("Missing property providers");
 if(!lookup(field))throw std::runtime_error("Missing original unnamed-property association");
 std::uint16_t zero=0;std::memcpy(reinterpret_cast<std::byte*>(field)+20,&zero,2);
 original(field,0x168,object);deserialize(field,object);
 if((original(field,0x268,0)&255)==0)initialize(field,object);
}
}
