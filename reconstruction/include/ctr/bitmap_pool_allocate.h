#pragma once
#include "ctr/bitmap_allocate.h"
#include "ctr/system_pool_backend.h"
namespace ctr {
// Concrete derived default014db610, successful positive1..128/align8/debug0.
BitmapAllocation allocate_derived_bitmap_pool(std::span<std::byte> derived,
 SystemPoolBackend& backend,std::uint64_t request,std::uint32_t alignment);
}
