#pragma once
#include "ctr/bitmap_normal_allocate.h"
namespace ctr {
// Exact singleton small-bin reuse while a sole large group3 remainder exists.
BitmapAllocation reuse_normal_bitmap_exact_small(std::span<std::byte> pool,SystemPoolBackend& backend,std::uint64_t request,std::uint32_t alignment);
}
