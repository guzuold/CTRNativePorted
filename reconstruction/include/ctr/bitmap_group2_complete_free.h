#pragma once
#include "ctr/bitmap_group2_prefix_insert.h"
namespace ctr {
// Only second256 after genuine first512..640 nonlastfree.
std::uint32_t free_normal_bitmap_group2_second(std::span<std::byte>,SystemPoolBackend&,std::uint64_t);
}
