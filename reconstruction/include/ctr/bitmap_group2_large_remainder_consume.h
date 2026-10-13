#pragma once
#include "ctr/bitmap_group2_split_large_remainder.h"
namespace ctr {
BitmapAllocation allocate_normal_bitmap_group2_large_remainder(std::span<std::byte>,SystemPoolBackend&,std::uint64_t,std::uint32_t);
}
