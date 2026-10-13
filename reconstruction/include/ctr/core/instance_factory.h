#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
std::uint64_t create_instance(std::span<const std::byte> metadata,std::span<const std::byte> allocator,std::uint32_t& aligned_failures,std::uint32_t& adjusted_failures);
}

