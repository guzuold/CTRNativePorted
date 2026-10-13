#pragma once
#include "ctr/system_pool_allocate.h"
namespace ctr {
// Only014d81d0 old-null branch; nonnull realloc/copy/free is not reconstructed.
SystemPoolAllocation reallocate_system_pool_from_null(std::span<std::byte> pool,
 SystemPoolBackend& backend,std::uint64_t request,std::uint32_t alignment,bool poison);
// Original014dabb0 modeflag1, old pointer/context null, positive bounded allocation.
SystemPoolAllocation dispatch_null_reallocation_system_pool(std::span<std::byte> pool,
 SystemPoolBackend& backend,std::uint64_t request,std::uint32_t alignment,bool poison);
}
