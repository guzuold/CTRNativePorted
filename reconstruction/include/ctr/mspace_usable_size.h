#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
namespace ctr {
// Read-only original libc+63000. Null is represented explicitly; malformed spans reject.
std::uint64_t mspace_usable_size(std::span<const std::byte> backing,
    std::optional<std::size_t> payload_offset);
}
