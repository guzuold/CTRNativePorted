#pragma once
#include "ctr/manager_pool_bootstrap.h"
namespace ctr {
// Construct real manager/pool; prepare activation but stop before015b52d0 factory.
SystemPoolAllocation begin_new_pool_activation_prefix(std::span<std::byte> descriptor,
 std::span<const std::byte> previous_manager,std::span<std::byte> previous_pool,
 SystemPoolBackend& backend,bool poison,std::uint64_t& publication);
}
