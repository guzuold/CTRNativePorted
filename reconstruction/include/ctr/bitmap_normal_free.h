#pragma once
#include "ctr/bitmap_normal_allocate.h"
namespace ctr {
// Normal allocations only; last allocated chunk adjoining the singleton large free chunk.
std::uint32_t free_last_normal_derived_bitmap_pool(std::span<std::byte> pool,
 SystemPoolBackend& backend,std::uint64_t pointer);
}
