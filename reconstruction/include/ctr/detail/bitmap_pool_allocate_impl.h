#pragma once
#include "ctr/bitmap_pool_allocate.h"
namespace ctr::detail {
BitmapAllocation allocate_bitmap_pool_bounded(std::span<std::byte> derived,
 SystemPoolBackend& backend,std::uint64_t underlying_request,std::uint64_t original_request,
 std::uint32_t alignment,bool requested_footer);
}
