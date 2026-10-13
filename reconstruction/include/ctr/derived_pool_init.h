#pragma once
#include "ctr/system_pool_mode1.h"
namespace ctr {
void construct_allocated_derived_pool(std::span<std::byte> pool,SystemPoolBackend& backend,
 std::uint64_t& publication);
// Only initial backing allocation, not derived resource/heap activation or full factory.
std::uint32_t initialize_derived_pool_backing(std::span<std::byte> derived,
 std::span<std::byte> parent,SystemPoolBackend& backend,std::uint64_t capacity,
 std::uint32_t requested_alignment,bool poison);
}
