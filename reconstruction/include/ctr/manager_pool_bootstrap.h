#pragma once
#include "ctr/descriptor_bootstrap_prefix.h"
namespace ctr {
//01bab668 through nested01bad14a, before registry-dependent pool activation.
SystemPoolAllocation begin_warm_manager_pool_bootstrap(std::span<std::byte> descriptor,
 std::span<const std::byte> previous_manager,std::span<std::byte> previous_pool,
 SystemPoolBackend& backend,bool poison,std::uint64_t& publication);
}
