// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/instance_copy_scalar_field.h"
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace ctr {
void instance_copy_scalar_field(std::span<const std::byte,24> field,
 std::span<std::byte> destination,std::span<const std::byte> source){
 std::uint16_t width,offset;std::memcpy(&width,field.data()+18,2);std::memcpy(&offset,field.data()+20,2);
 if(width!=1&&width!=2&&width!=4&&width!=8)throw std::runtime_error("Missing original field memcpy domain");
 if(offset>destination.size()||width>destination.size()-offset||offset>source.size()||width>source.size()-offset)
  throw std::runtime_error("Missing field extent");
 std::memcpy(destination.data()+offset,source.data()+offset,width);
}
}
