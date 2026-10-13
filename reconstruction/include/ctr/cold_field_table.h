#pragma once
#include "ctr/cold_typed_fields.h"
#include <array>
namespace ctr {
struct ColdFieldTable {SystemPoolAllocation table;std::array<SystemPoolAllocation,16> fields;};
ColdFieldTable construct_cold_generated_field_table(std::span<std::byte> metadata,
 std::span<const std::byte> descriptor,std::span<const std::byte> manager,
 std::span<std::byte> pool,SystemPoolBackend& backend,bool poison);
}
