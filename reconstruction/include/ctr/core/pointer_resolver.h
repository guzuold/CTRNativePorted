#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
std::uint64_t resolve_registered_pointer(std::span<const std::byte> metadata,
 std::uint64_t key, std::int32_t count, std::span<const std::byte> records);
}
