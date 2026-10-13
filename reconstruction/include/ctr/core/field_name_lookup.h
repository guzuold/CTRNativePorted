// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
#include <span>
namespace ctr {
struct FieldNameEntry { std::uintptr_t field; std::uintptr_t name; };
std::uintptr_t field_name_lookup(std::uintptr_t field, std::uint32_t flags,
 std::span<const FieldNameEntry> entries);
}
