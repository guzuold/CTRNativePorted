#pragma once
#include "ctr/bitmap_large_backward.h"
namespace ctr {
BitmapAllocation allocate_normal_bitmap_large_reuse_align16(std::span<std::byte>,SystemPoolBackend&,std::uint64_t,std::uint32_t);
}
