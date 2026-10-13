#pragma once
#include "ctr/pooled_name_reference.h"
namespace ctr {
// Full0153f260 for exactly one scalar field, inherited0, explicitly bound nonnull type cache.
void register_single_cached_scalar_field(std::span<std::byte> field,std::uint64_t field_token,
 std::span<std::byte> metadata,std::span<const std::byte> context,
 const SystemPoolAllocation& table,const PooledNameInsertion& name,std::string_view text,
 std::uint16_t offset,std::uint64_t cached_type_token,std::span<const std::byte> cached_type,
 std::span<std::byte> publication);
}
