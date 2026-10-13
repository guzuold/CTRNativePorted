#pragma once
#include "ctr/system_pool_allocate.h"
namespace ctr {
// 014dabb0 mode1 / 014d81d0 copy branch on a single prior small allocation.
SystemPoolAllocation dispatch_copy_reallocation_system_pool(
 std::span<std::byte> pool,SystemPoolBackend& backend,
 const SystemPoolAllocation& old,std::uint64_t request,std::uint32_t alignment,bool poison);
}
