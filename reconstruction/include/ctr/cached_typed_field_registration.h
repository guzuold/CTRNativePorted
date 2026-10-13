#pragma once
#include "ctr/cached_scalar_field_registration.h"
namespace ctr {
// Known Int/UShort/ObjectRef/RawRef/Vector; single field/inherited0/explicit nonnull cache.
void register_single_cached_typed_field(std::span<std::byte> field,std::uint64_t field_token,
 std::span<std::byte> metadata,std::span<const std::byte> context,
 const SystemPoolAllocation& table,const PooledNameInsertion& name,std::string_view text,
 std::uint16_t offset,std::uint64_t cached_type_token,std::span<const std::byte> cached_type,
 std::span<std::byte> publication);
}
