#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Whole014dabb0: only original refusal paths are supported; active providers remain missing.
std::uint64_t dispatch_allocation(std::span<const std::byte> descriptor, std::uint64_t size, std::uint64_t argument);
}
