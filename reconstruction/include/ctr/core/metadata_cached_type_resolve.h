// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Explicit resolved cache input; registry lookup and bootstrap remain missing.
std::uintptr_t metadata_cached_type_resolve(std::uintptr_t cached,
 std::span<const std::byte> type);
}
