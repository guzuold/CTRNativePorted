#pragma once
#include "ctr/cold_name_pool_create.h"
namespace ctr {
// Full015a7250 on the actual64-byte cold-created context/System Pool1.
void activate_created_name_pool(std::span<std::byte> object,
 std::span<const std::byte> manager,std::span<std::byte> pool,
 std::uint8_t global_selector,std::int32_t flag);
}
