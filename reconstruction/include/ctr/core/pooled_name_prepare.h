#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Whole00a50010; nonnull text requires the missing original pool service.
void prepare_pooled_name(std::span<std::byte> destination, std::uint64_t text_token);
}
