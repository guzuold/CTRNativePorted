#pragma once
#include "ctr/bitmap_normal_allocate.h"
namespace ctr {
// Exact small reuse after one actual normal112 fallback separates the allocated neighbor from the large tail.
BitmapAllocation reuse_normal_bitmap_small_after_fallback(std::span<std::byte> pool,SystemPoolBackend& backend,std::uint64_t request,std::uint32_t alignment);
}
