#pragma once
#include <cstdint>
#include <span>
#include <cstddef>
namespace ctr {
void set_prepared_vector_descriptor_default(std::span<std::byte> field,
 std::uint64_t backing_token,std::span<std::byte> backing,
 std::uint64_t old_token,std::span<std::byte> old_object,
 std::uint64_t new_token,std::span<std::byte> new_object);
}
