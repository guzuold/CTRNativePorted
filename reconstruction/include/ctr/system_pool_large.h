#pragma once
#include "ctr/system_pool_mode1.h"
namespace ctr {
// Distinct contract: exactly8192 bytes, successful top-only allocation, old/context null.
SystemPoolAllocation dispatch_system_pool_resource_backing(std::span<std::byte> pool,
 SystemPoolBackend& backend,std::uint64_t request,std::uint32_t alignment,bool poison);
}
