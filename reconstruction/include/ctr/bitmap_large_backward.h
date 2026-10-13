#pragma once
#include "ctr/bitmap_normal_large.h"
namespace ctr {
// Exactly two adjacent normal allocations: first256..384, second256,align8.
BitmapAllocation allocate_normal_bitmap_large_pair(std::span<std::byte>,SystemPoolBackend&,std::uint64_t,std::uint32_t);
std::uint32_t free_normal_bitmap_large_pair(std::span<std::byte>,SystemPoolBackend&,std::uint64_t);
}
