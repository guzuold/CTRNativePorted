#pragma once
#include "ctr/bitmap_group2_split_large_remainder.h"
namespace ctr {
std::uint32_t free_normal_bitmap_group2_split_forward_large(std::span<std::byte>,SystemPoolBackend&,std::uint64_t);
}
