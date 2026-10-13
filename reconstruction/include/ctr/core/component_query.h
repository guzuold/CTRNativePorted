#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Prepared address image; supported virtual getter is the original igObject getter.
std::uint64_t query_registered_component(std::span<const std::byte> image,
 std::uint64_t base, std::uint64_t object, std::uint64_t expected,
 std::uint32_t ordinal, std::uint64_t igobject_type);
}
