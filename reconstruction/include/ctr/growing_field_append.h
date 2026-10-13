#pragma once
#include "ctr/populated_field_reserve.h"
#include "ctr/reserved_field_append.h"
namespace ctr {
SystemPoolAllocation append_growing_metadata_field(std::span<std::byte> metadata,
 std::span<const std::byte> manager,std::span<std::byte> pool,SystemPoolBackend& backend,
 const SystemPoolAllocation& table,std::span<std::byte> field,std::uint64_t field_token,bool poison);
}
