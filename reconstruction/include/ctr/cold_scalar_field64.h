#pragma once
#include "ctr/system_pool_allocate.h"
namespace ctr {
SystemPoolAllocation create_cold_scalar_field64(std::span<const std::byte> descriptor,
 std::span<const std::byte> manager,std::span<std::byte> pool,SystemPoolBackend& backend,bool poison);
}
