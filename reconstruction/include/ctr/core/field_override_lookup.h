#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Original0153cce0: borrowed opaque value, no ownership transfer.
std::uint64_t lookup_field_override(std::span<const std::byte> field,
 std::uint64_t field_token,std::int32_t count,std::span<const std::byte> pairs);
}
