#pragma once
#include "ctr/bitmap_large_reuse_align16.h"
namespace ctr {
BitmapAllocation allocate_normal_bitmap_align16_leading_gap(std::span<std::byte>,SystemPoolBackend&,std::uint64_t,std::uint32_t);
}
