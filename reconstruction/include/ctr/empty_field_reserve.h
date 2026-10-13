#pragma once
#include "ctr/system_pool_mode1.h"
namespace ctr {
// Full0109e120/014f4e00 empty tagged pool1 array,1..64 pointer slots.
SystemPoolAllocation reserve_empty_field_array(std::span<std::byte> header,
 std::span<const std::byte> manager,std::span<std::byte> pool,
 SystemPoolBackend& backend,std::uint32_t fields,bool poison);
}
