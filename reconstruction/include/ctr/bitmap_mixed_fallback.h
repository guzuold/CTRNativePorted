#pragma once
#include "ctr/bitmap_normal_allocate.h"
namespace ctr {
// Normal112-byte fallback from an insufficient singleton small bin to the sole large group3 tail.
BitmapAllocation allocate_normal_bitmap_large_fallback(std::span<std::byte> pool,SystemPoolBackend& backend,std::uint64_t request,std::uint32_t alignment);
}
