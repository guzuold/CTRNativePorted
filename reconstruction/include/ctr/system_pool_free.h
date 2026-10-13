#pragma once
#include "ctr/system_pool_allocate.h"
namespace ctr {
std::uint64_t system_pool_requested_size(std::span<const std::byte> backing,std::uint64_t address);
// Successful nonnull LIFO free wrapper only; original diagnostics/status failures excluded.
std::uint32_t free_system_pool_top(std::span<std::byte> pool,SystemPoolBackend& backend,
 std::uint64_t address,bool poison);
}
