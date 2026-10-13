#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
struct MspaceLayoutParameters {
    std::uint64_t base_token{},capacity{};
    bool owns_backing{};
    std::uint8_t poison{},debug_flags{},page_scale{},granularity_scale{};
};
// libc.prx +6041d..+60510 only; produces metadata, not a usable allocator.
std::uint32_t initialize_mspace_layout(std::span<std::byte> backing,
    const MspaceLayoutParameters& parameters);
}
