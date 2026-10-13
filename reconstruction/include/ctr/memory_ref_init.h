#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// 0154b3c0 with original igMemoryRef[Array]MetaField size providers.
// Fresh storage only: this does not release an existing owned memory reference.
std::uint16_t initialize_zero_memory_refs(std::span<std::byte> storage,
    std::uint16_t offset, bool array, std::uint16_t element_count);
}
