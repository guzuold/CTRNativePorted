#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Only original015c3270 successful aligned header constructor; not a usable heap.
std::uint64_t initialize_bitmap_resource_header(std::span<std::byte> storage);
}
