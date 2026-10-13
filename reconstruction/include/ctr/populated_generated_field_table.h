#pragma once
#include "ctr/cold_field_table.h"
namespace ctr {
ColdFieldTable append_cold_generated_fields(std::span<std::byte> metadata,
 std::span<const std::byte> descriptor,std::span<const std::byte> manager,
 std::span<std::byte> pool,SystemPoolBackend& backend,const SystemPoolAllocation& previous,bool poison);
}
