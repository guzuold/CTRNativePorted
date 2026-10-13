#pragma once
#include "ctr/bitmap_group2_prefix_insert.h"
namespace ctr {
// Same first request512..640 after genuine prefixfree; measured prefix or tail choice.
BitmapAllocation allocate_normal_bitmap_group2_reallocation(std::span<std::byte>,SystemPoolBackend&,std::uint64_t,std::uint32_t);
}
