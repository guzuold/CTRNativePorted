#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
namespace ctr {
// Metadata-supplied offset is an explicit input, not a guessed class layout.
std::uint64_t capture_model_dispatch(std::span<const std::byte> model,
    std::int32_t dispatch_offset);
// Only the original post-allocation header writes. Does not allocate, initialize
// reflected fields or return a usable actor/physics body.
void initialize_factory_header(std::span<std::byte> storage,
    std::int32_t dispatch_offset, std::uint64_t dispatch,
    std::optional<std::uint32_t> pool_index);
}
