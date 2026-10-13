#pragma once
#include "ctr/system_pool_backend.h"
namespace ctr {
struct SystemPoolAllocation {
 std::uint64_t raw_address{},address{},usable_size{};
 std::span<std::byte> payload;
};
// Concrete system-pool wrapper, successful positive request on verified top-only heap.
SystemPoolAllocation allocate_system_pool(std::span<std::byte> pool,SystemPoolBackend& backend,
 std::uint64_t request,std::uint32_t alignment,bool poison);
}
