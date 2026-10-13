#pragma once
#include "ctr/system_pool_grow_reallocation.h"
namespace ctr {
SystemPoolAllocation reserve_populated_field_array(std::span<std::byte> header,
 std::span<const std::byte> manager,std::span<std::byte> pool,SystemPoolBackend& backend,
 const SystemPoolAllocation& old,std::uint32_t fields,bool poison);
}
