#pragma once
#include "ctr/bitmap_pool_allocate.h"
namespace ctr {
//014dabb0 concrete derived default path, mode0/old-null/context-null/minimum8.
BitmapAllocation dispatch_new_derived_bitmap_allocation(std::span<std::byte> derived,
 SystemPoolBackend& backend,std::uint64_t request,std::uint32_t alignment);
}
