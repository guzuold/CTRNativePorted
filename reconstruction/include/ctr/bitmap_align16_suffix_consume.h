#pragma once
#include "ctr/bitmap_align16_prefix_consume.h"
namespace ctr {
BitmapAllocation allocate_normal_bitmap_align16_suffix(std::span<std::byte>,SystemPoolBackend&,std::uint64_t,std::uint32_t);
std::uint64_t bitmap_align16_suffix_requested_size(std::span<std::byte>,SystemPoolBackend&,std::uint64_t);
}
