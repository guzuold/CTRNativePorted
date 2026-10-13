// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_identifier_lookup.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uintptr_t metadata_identifier_lookup(std::span<const std::byte,120> registry,std::int32_t index){
 if(registry[0]==std::byte{0})return 0;
 std::uintptr_t sync{},container{},backing{},result{};
 std::memcpy(&sync,registry.data()+0x70,8);
 if(sync && *reinterpret_cast<const unsigned char*>(sync+0x18))throw std::runtime_error("Missing original synchronization1adb0a0/1adae50");
 std::memcpy(&container,registry.data()+0x20,8);
 if(!container)throw std::invalid_argument("Registry backing container missing");
 std::memcpy(&backing,reinterpret_cast<const void*>(container+0x10),8);
 if(!backing)throw std::invalid_argument("Registry pointer array missing");
 std::memcpy(&result,reinterpret_cast<const std::byte*>(backing)+static_cast<std::ptrdiff_t>(index)*8,8);
 return result;
}
}
