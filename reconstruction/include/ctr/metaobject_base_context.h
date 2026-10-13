#pragma once
#include "ctr/system_pool_mode1.h"
namespace ctr {
// Original01543420, prepared nonthreaded manager/selector1/concrete SystemPool.
SystemPoolAllocation create_metaobject_base_context(std::span<const std::byte> manager,
 std::span<std::byte> pool,SystemPoolBackend& backend,bool poison);
}
