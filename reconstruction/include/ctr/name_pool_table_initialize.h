#pragma once
#include "ctr/cold_name_pool_create.h"
namespace ctr {
// Full015a7030 nonthreaded initial1024 buckets, actual8192-byte allocation.
SystemPoolAllocation initialize_created_name_pool_table(std::span<std::byte> object,
 std::span<const std::byte> manager,std::span<std::byte> pool,
 SystemPoolBackend& backend,bool poison);
}
