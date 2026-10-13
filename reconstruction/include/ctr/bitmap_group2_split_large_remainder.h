#pragma once
#include "ctr/bitmap_group2_post_reallocation_cycle.h"
namespace ctr {
// Genuine group2prefix520..648 split to allocation144..200 and largegroup1 remainder.
BitmapAllocation allocate_normal_bitmap_group2_split_large_remainder(std::span<std::byte>,SystemPoolBackend&,std::uint64_t,std::uint32_t);
}
