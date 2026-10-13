// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/field_default_initialize.h"
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace ctr {
void field_default_initialize(std::span<const std::byte,64> field,std::span<std::byte> object){
 std::uint32_t flags{};std::uint16_t width{},offset{};std::uintptr_t vtable{},target{},value{};std::uint64_t attributes{};
 std::memcpy(&flags,field.data()+32,4);std::memcpy(&width,field.data()+18,2);std::memcpy(&offset,field.data()+20,2);
 if(((flags>>2)&3)!=0)throw std::runtime_error("Missing original alternate default mode");
 std::memcpy(&vtable,field.data(),8);if(!vtable)throw std::runtime_error("Missing original field vtable");
 std::memcpy(&target,reinterpret_cast<const std::byte*>(vtable)+0x140,8);
 if(target!=0x4013de020ull)throw std::runtime_error("Missing original default virtual provider");
 std::memcpy(&attributes,field.data()+48,8);std::memcpy(&value,field.data()+56,8);
 if(width!=1&&width!=2&&width!=4&&width!=8)throw std::runtime_error("Missing original default memcpy/memset");
 if(offset>object.size()||width>object.size()-offset)throw std::runtime_error("Missing object extent");
 const std::byte* source=(attributes&(1ull<<56))&&width<=4?field.data()+56:reinterpret_cast<const std::byte*>(value);
 if(source){if(width==8)throw std::runtime_error("Missing original default memcpy");std::memcpy(object.data()+offset,source,width);}
 else {if(width==2)throw std::runtime_error("Missing original default memset");std::memset(object.data()+offset,0,width);}
}
}
