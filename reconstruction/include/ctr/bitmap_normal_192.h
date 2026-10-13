#pragma once
#include "ctr/bitmap_normal_allocate.h"
namespace ctr {
BitmapAllocation allocate_normal_bitmap_192(std::span<std::byte> pool,SystemPoolBackend& backend,std::uint64_t request,std::uint32_t alignment);
std::uint64_t bitmap_requested_size_192(std::span<const std::byte> resource,std::uint64_t pointer);
std::uint32_t free_last_normal_bitmap_192(std::span<std::byte> pool,SystemPoolBackend& backend,std::uint64_t pointer);
}
