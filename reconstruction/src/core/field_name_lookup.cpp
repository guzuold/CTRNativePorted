// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/field_name_lookup.h"
namespace ctr {
std::uintptr_t field_name_lookup(std::uintptr_t field,std::uint32_t flags,
 std::span<const FieldNameEntry> entries){
 if((flags&0x100000)==0)return 0;
 for(const auto& entry:entries)if(entry.field==field)return entry.name;
 return 0;
}
}
