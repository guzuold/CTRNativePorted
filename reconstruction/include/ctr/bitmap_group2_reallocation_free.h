#pragma once
#include "ctr/bitmap_group2_reallocation.h"
namespace ctr {
std::uint32_t free_normal_bitmap_group2_reallocation(std::span<std::byte>,SystemPoolBackend&,std::uint64_t);
}
