#pragma once
#include "ctr/core/scalar_field_reset.h"
namespace ctr {
struct ConstructionFieldBinding {
 std::uint64_t token;
 std::span<const std::byte> field;
 std::span<const std::byte> backing;
 std::uint64_t backing_token;
};
// Whole014dc920 with bound known scalar/raw providers; unknown hooks remain missing.
void construct_scalar_field_object(std::span<std::byte> object,
 std::span<std::byte> metadata,std::span<const std::byte> descriptor,
 std::span<const std::byte> base_metadata,std::span<const std::byte> pool,
 std::span<const std::byte> table,std::uint64_t table_token,
 std::span<const ConstructionFieldBinding> fields);
}
