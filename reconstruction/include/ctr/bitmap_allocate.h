#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
struct BitmapAllocation {std::uint64_t address{};std::span<std::byte> payload;};
// Original015c3690 positive1..128, alignment8, singleton large free chunk with split.
BitmapAllocation allocate_bitmap_singleton(std::span<std::byte> resource,
 std::uint64_t request,std::uint32_t alignment);
}
