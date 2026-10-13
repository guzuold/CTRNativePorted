#pragma once
#include "ctr/bitmap_pool_allocate.h"
namespace ctr {
BitmapAllocation allocate_normal_derived_bitmap_pool(std::span<std::byte> derived,
 SystemPoolBackend& backend,std::uint64_t request,std::uint32_t alignment);
// Original014db250 positive actual normal allocation; null/fault paths excluded.
std::uint64_t bitmap_normal_requested_size(std::span<const std::byte> resource,std::uint64_t pointer);
}
