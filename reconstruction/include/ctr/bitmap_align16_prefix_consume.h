#pragma once
#include "ctr/bitmap_align16_leading_gap.h"
namespace ctr { BitmapAllocation allocate_normal_bitmap_align16_prefix(std::span<std::byte>,SystemPoolBackend&,std::uint64_t,std::uint32_t); }
