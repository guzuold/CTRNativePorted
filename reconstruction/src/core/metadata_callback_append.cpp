// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_callback_append.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void metadata_callback_append(std::span<std::byte,16> list,std::uintptr_t callback){
 std::uintptr_t backing{};std::int32_t count{},capacity{};
 std::memcpy(&backing,list.data(),8);std::memcpy(&count,list.data()+8,4);std::memcpy(&capacity,list.data()+12,4);
 if(count==capacity)throw std::runtime_error("Missing original pool/context growth13d8550/13dabb0");
 if(!backing||count<0||count>capacity)throw std::runtime_error("Missing invalid list domain");
 const auto next=static_cast<std::uint32_t>(count)+1u;
 std::memcpy(list.data()+8,&next,4);
 std::memcpy(reinterpret_cast<std::byte*>(backing)+static_cast<std::size_t>(count)*8,&callback,8);
}
}
