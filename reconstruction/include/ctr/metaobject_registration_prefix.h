#pragma once
#include "ctr/metaobject_allocation_prefix.h"
namespace ctr {
// Original0153fd20 prefix only: returns next original branch, never service success.
std::uint32_t begin_metaobject_registration_flags(std::span<std::byte> metadata);
// Null parent, empty separate cell, prepared nonthreaded selector1 cold branch.
SystemPoolAllocation begin_cold_metaobject_registration(std::span<const std::byte> descriptor,
 std::span<const std::byte> manager,std::span<std::byte> pool,SystemPoolBackend& backend,
 bool poison,std::uint64_t& registration_cell);
}
