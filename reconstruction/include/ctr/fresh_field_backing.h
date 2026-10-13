#pragma once
#include "ctr/system_pool_mode1.h"
namespace ctr {
//014e38c0 fresh noninline branch; prepared slot0, actual System Pool, known types.
SystemPoolAllocation initialize_fresh_field_backing(std::span<std::byte> field,
 std::span<const std::byte> manager,std::span<std::byte> pool,
 SystemPoolBackend& backend,bool poison);
}
