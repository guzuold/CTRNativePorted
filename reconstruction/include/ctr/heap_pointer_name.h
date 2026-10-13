#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Original bounded libc+5ecef..5edfd, prepared prefix and original strlen result.
void append_heap_pointer_name(std::span<std::byte> name,std::uint32_t prefix_length,std::uint64_t token);
}
