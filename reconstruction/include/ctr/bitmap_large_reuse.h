#pragma once
#include "ctr/bitmap_large_backward.h"
namespace ctr {
BitmapAllocation allocate_normal_bitmap_exact_large_reuse(std::span<std::byte>,SystemPoolBackend&,std::uint64_t,std::uint32_t);
}
