#pragma once
#include "ctr/empty_field_reserve.h"
namespace ctr {
SystemPoolAllocation reserve_metadata_field_array(std::span<std::byte> metadata,
 std::span<const std::byte> manager,std::span<std::byte> pool,
 SystemPoolBackend& backend,std::uint32_t fields,bool poison);
void append_reserved_metadata_field(std::span<std::byte> metadata,
 const SystemPoolAllocation& table,std::span<std::byte> field,std::uint64_t field_token);
}
