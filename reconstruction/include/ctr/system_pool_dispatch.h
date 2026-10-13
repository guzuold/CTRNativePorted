#pragma once
#include "ctr/system_pool_allocate.h"
namespace ctr {
// Original014dabb0 concrete override: old pointer null, positive size, mode flag0/context null.
SystemPoolAllocation dispatch_new_system_pool_allocation(std::span<std::byte> pool,
 SystemPoolBackend& backend,std::uint64_t request,std::uint32_t alignment,bool poison);
}
