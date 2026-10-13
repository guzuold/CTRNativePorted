// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "field_name_lookup.h"
#include <cstddef>
#include <functional>
namespace ctr {
using FieldNameRelease=std::function<void(std::uintptr_t,std::uintptr_t)>;
void field_name_assign(std::span<std::byte,36> field,std::uintptr_t identity,
 std::uintptr_t* incoming,std::span<FieldNameEntry> entries,std::uintptr_t context,
 const FieldNameRelease& release);
}
