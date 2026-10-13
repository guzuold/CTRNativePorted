#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Original 01539610 warm nonthreaded path through 015396a3, before allocation.
// Supplied registry/manager are explicit inputs, not initialized engine services.
std::uint64_t select_descriptor_manager_pool(std::span<const std::byte> descriptor,
    std::span<const std::byte> manager, std::int32_t selector);
}
