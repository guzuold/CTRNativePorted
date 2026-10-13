#pragma once
#include "ctr/generated_field_registration.h"
#include "ctr/inherited_field_selection.h"
namespace ctr {
void register_inherited_generated_fields(ColdFieldTable& fields,std::span<std::byte> metadata,
 std::span<std::byte> names,const SystemPoolAllocation& buckets,std::span<std::byte> pool,
 SystemPoolBackend& backend,std::span<const std::byte> cached_types,
 std::span<const std::uint16_t,16> offsets,std::span<std::byte> publications,
 std::span<const InheritedFieldBinding> inherited,bool poison);
}
