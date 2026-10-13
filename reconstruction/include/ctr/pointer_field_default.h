#pragma once
#include <cstdint>
#include <span>
#include <cstddef>
namespace ctr {
// Explicit backing binding only: no unknown object/handle resolver.
void set_prepared_pointer_field_default(std::span<std::byte> field,
 std::uint64_t backing_token,std::span<std::byte> backing,
 std::span<const std::byte> value,bool has_value);
}
