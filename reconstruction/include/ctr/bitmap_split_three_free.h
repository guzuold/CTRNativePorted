#pragma once
#include "ctr/bitmap_split_remainder_reuse.h"
namespace ctr {
std::uint32_t free_normal_bitmap_split_three(std::span<std::byte>,SystemPoolBackend&,std::uint64_t);
}
