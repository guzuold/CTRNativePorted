#pragma once
#include <span>
#include <cstddef>
#include <cstdint>
namespace ctr {
struct AllocationPoolView { std::uint64_t address; std::span<const std::byte> bytes; };
std::uint64_t allocation_owner_query(std::span<const std::byte> manager,
 std::uint64_t allocation,std::uint32_t mode, std::span<const AllocationPoolView> pools = {});
}

