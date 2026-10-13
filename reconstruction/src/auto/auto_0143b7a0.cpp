// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/auto/auto_0143b7a0.hpp"
#include "ctr/core/metadata_field_compose.h"
namespace ctr {
void auto_0143b7a0(std::span<std::byte,152> metadata,
 std::span<std::byte,64> table,std::span<std::byte,80> field,
 std::uintptr_t field_type,std::uintptr_t special_type,
 const std::byte* value_type,const std::byte* registry){field[34]|=std::byte{0x80};metadata_field_compose_default8(metadata,table,field,field_type,special_type,value_type,registry); }
}
