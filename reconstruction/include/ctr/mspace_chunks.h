#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
struct MspaceChunksParameters {
    std::uint64_t base_token{};
    std::uint64_t capacity{};
    std::uint32_t header_offset{};
    std::uint8_t global_lock_policy{};
    bool permit_mutex_initialization{};
};
// Original libc chunk construction; mutex flag requires explicit caller-owned initialization.
void initialize_mspace_chunks(std::span<std::byte> backing,const MspaceChunksParameters& parameters);
// Original libc +609bb..+60a41, including +60adc on bitmap exhaustion.
// External bitmap/table lifetime must already be established.
void register_mspace(std::span<std::byte> backing,std::uint64_t base_token,
    std::uint32_t header_offset,std::uint32_t initial_padding,
    std::uint64_t* bitmap,std::span<std::uint64_t> table);
}
