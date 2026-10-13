#pragma once
#include "ctr/system_pool_allocate.h"
namespace ctr {
SystemPoolAllocation dispatch_grow_reallocation_system_pool(std::span<std::byte> pool,
 SystemPoolBackend& backend,const SystemPoolAllocation& old,std::uint64_t request,
 std::uint32_t alignment,bool poison);
}
