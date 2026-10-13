#pragma once
#include "ctr/descriptor_manager_allocation.h"
#include "ctr/game_global_malloc.h"
namespace ctr {
MspaceAllocation begin_cold_descriptor_bootstrap(std::span<std::byte> descriptor,
 const PreparedGameGlobalMalloc& binding,std::uint64_t& publication);
// Bounded warm/nonthreaded01bab668..01bab6ce; subsequent registry work missing.
SystemPoolAllocation begin_warm_descriptor_bootstrap(std::span<std::byte> descriptor,
 std::span<const std::byte> current_manager,std::span<std::byte> pool,
 SystemPoolBackend& backend,bool poison,std::uint64_t& publication);
}
