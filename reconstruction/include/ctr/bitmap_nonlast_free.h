#pragma once
#include "ctr/bitmap_normal_free.h"
namespace ctr {
// Penultimate small insertion, or final allocated neighbor merging backward small and forward large.
std::uint32_t free_normal_bitmap_adjacent_pair(std::span<std::byte> pool,SystemPoolBackend& backend,std::uint64_t pointer);
}
