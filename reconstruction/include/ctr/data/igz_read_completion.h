#pragma once
#include <cstdint>
#include <optional>
#include <span>
namespace ctr::data {
// Original 0157ce30: status at request+0x50, signed list count at +0xc,
// pointer array at +0x20. Missing primary means error; missing child is ready.
std::uint32_t igz_read_completion(std::optional<std::uint32_t> primary,
    std::span<const std::optional<std::uint32_t>> children, bool& ready);
}
