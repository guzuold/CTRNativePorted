#pragma once
#include "ctr/system_pool_mode1.h"
namespace ctr {
// Full015a7290 cold path; native allocation result is a logical API output.
SystemPoolAllocation create_cold_name_pool(std::span<const std::byte> descriptor,
 std::span<const std::byte> manager,std::span<std::byte> pool,
 SystemPoolBackend& backend,bool poison,std::uint8_t global_selector,
 std::uint64_t& publication,std::span<std::byte> previous,std::uint64_t previous_token);
}
