#pragma once
#include "ctr/system_pool_dispatch.h"
#include <string_view>
namespace ctr {
struct PooledNameInsertion { SystemPoolAllocation record; std::uint64_t string_address; };
// Original015a74c0: fresh nonthreaded16-bucket pool, bounded string/no rehash.
PooledNameInsertion insert_fresh_pooled_name(std::span<std::byte> strings,
 std::span<std::byte> buckets,std::uint64_t buckets_address,std::span<std::byte> pool,
 SystemPoolBackend& backend,std::string_view name,bool poison);
}
