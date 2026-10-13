#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Bounded, aligned raw resource only. No allocation/free or derived lifecycle.
std::uint64_t initialize_bitmap_resource_chunk(std::span<std::byte> resource);
std::uint64_t create_bitmap_resource(std::span<std::byte> resource);
}
