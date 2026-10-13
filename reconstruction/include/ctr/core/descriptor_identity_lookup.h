// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
bool descriptor_identity_lookup(std::span<const std::byte,48> table,const std::uint64_t& query,std::uint32_t hash,std::uintptr_t& output,const std::uint64_t& sentinel,const std::uint8_t& initialized);
}
