// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <span>
#include <cstddef>
#include <cstdint>
namespace ctr {
// Resolved default8 descriptor family (retail vtable25bba10).
// Cached type arguments are the values loaded by original type0/c0 getters.
void metadata_field_compose_default8(std::span<std::byte,152> metadata,
 std::span<std::byte,64> table,std::span<std::byte,80> field,
 std::uintptr_t field_type,std::uintptr_t special_type,
 const std::byte* value_type,const std::byte* registry=nullptr);
}
