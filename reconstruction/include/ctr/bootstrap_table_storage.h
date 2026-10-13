#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Nonthreaded post-allocation cold constructor; global pointer null or same object.
void initialize_bootstrap_table_storage(std::span<std::byte> storage);
void assign_bootstrap_table_pool_index(std::span<std::byte> storage,std::uint32_t index);
}
