#pragma once
#include <span>
#include <cstddef>
namespace ctr {
// Prepared inline scalar storage only; missing allocator/owner services reject.
void set_inline_scalar_field_default(std::span<std::byte> field,
 std::span<const std::byte> value,bool has_value);
void initialize_fresh_scalar_field_default(std::span<std::byte> field,
 std::span<const std::byte> manager,std::span<const std::byte> pool,
 std::span<const std::byte> value,bool has_value);
}
