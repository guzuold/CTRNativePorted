#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Only original post-allocation01538769..015387d2, before bootstrap call.
void initialize_allocated_dispatch_descriptor(std::span<std::byte> storage,std::uint64_t& publication);
}
