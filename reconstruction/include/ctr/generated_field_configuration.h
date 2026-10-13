#pragma once
#include "ctr/cold_field_table.h"
namespace ctr {
// Prepared cached canonical/metadata only; callback01bb638b..6559 configuration.
void configure_generated_metaobject_fields(ColdFieldTable& table,
 std::span<const std::byte> manager,std::span<std::byte> pool,
 std::uint64_t metadata_token,std::uint64_t canonical_token,
 std::span<const std::byte> canonical,std::uint64_t element_type,
 std::uint64_t attribute_type,std::uint64_t base_type);
}
