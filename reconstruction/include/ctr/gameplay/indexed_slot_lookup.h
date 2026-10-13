// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
#include <span>
namespace ctr {
std::uint64_t indexed_slot_lookup(std::uint32_t token,std::uint32_t sentinel,std::int32_t count,
 std::span<const std::uint16_t> tags,std::span<const std::uint64_t> values);
}
