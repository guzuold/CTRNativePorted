#pragma once
#include "ctr/bitmap_large_split_reuse.h"
namespace ctr {
BitmapAllocation allocate_normal_bitmap_split_remainder(std::span<std::byte>,SystemPoolBackend&,std::uint64_t,std::uint32_t);
std::uint64_t bitmap_split_remainder_requested_size(std::span<const std::byte>,std::uint64_t);
}
