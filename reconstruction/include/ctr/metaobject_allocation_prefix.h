#pragma once
#include "ctr/system_pool_mode1.h"
namespace ctr {
// Full concrete cold getter; original025ba2e8+40 resolves to014d7730 RET.
SystemPoolAllocation create_cold_metaobject(std::span<const std::byte> descriptor,
 std::span<const std::byte> manager,std::span<std::byte> pool,
 SystemPoolBackend& backend,bool poison);
// Cold getter through0156bb97, before virtual+40; prepared constructor selector1.
SystemPoolAllocation allocate_cold_metaobject_prefix(std::span<const std::byte> descriptor,
 std::span<const std::byte> manager,std::span<std::byte> pool,
 SystemPoolBackend& backend,bool poison);
}
