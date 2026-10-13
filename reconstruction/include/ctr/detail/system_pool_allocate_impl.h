#pragma once
#include "ctr/system_pool_allocate.h"
namespace ctr::detail {
// Internal limit supplied only by separately verified public contracts.
SystemPoolAllocation allocate_system_pool_bounded(std::span<std::byte> pool,
 SystemPoolBackend& backend,std::uint64_t request,std::uint32_t alignment,bool poison,
 std::uint64_t limit);
}
