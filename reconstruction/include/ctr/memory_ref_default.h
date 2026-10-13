#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
namespace ctr {
// 014dc540: initialized manager, non-threaded pool lookup, resolved field metadata.
// Slots are explicit pool indices, not allocator implementations.
void initialize_memory_ref_default(std::span<std::byte> storage,
    std::uint16_t offset, std::int32_t alignment,
    const std::array<std::optional<std::int32_t>,4>& pool_indices);
}
