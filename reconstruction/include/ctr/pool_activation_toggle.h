#pragma once
#include <span>
#include <cstddef>
#include <cstdint>
namespace ctr {
// Full015a7250, bounded resolved nonthreaded manager/pool storage.
void toggle_nonthreaded_pool_activation(std::span<std::byte> storage,
 std::uint64_t storage_token,std::int32_t flag,std::uint8_t global_selector);
}
