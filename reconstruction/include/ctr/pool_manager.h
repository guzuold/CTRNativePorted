#pragma once
#include <cstddef>
#include <span>
namespace ctr {
// Original 014ed4b0: rejects owned containers; preserves unknown layout bytes.
void reset_unowned_pool_manager(std::span<std::byte> manager);
// Original 01538590: base pool defaults with absent owner at +0x10.
void initialize_unowned_pool_storage(std::span<std::byte> pool);
}
