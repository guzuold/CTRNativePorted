// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/field_deserialize.h"
#include <stdexcept>
namespace ctr {
void field_deserialize(std::span<const std::byte,65> field,std::span<std::byte> object){
 (void)object;
 if(field[64]!=std::byte{0})throw std::runtime_error("Missing original deserialize name/factory/lifetime providers");
}
}
