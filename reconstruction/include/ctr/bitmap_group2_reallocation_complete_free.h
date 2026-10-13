#pragma once
#include "ctr/bitmap_group2_reallocation_free.h"
namespace ctr {
// Only remaining second256 after genuine same-request group2 reallocation/free.
std::uint32_t free_normal_bitmap_group2_reallocated_second(std::span<std::byte>,SystemPoolBackend&,std::uint64_t);
}
