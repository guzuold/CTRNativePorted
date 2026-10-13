#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Whole0153bc40. Record includes the 16-byte header before its string.
// Single-threaded prepared record only; replacing an owned name requires release.
void assign_pooled_name(std::span<std::byte> destination,
                       std::span<const std::byte> source_cell,
                       bool same_cell, std::span<std::byte> record,
                       std::uint64_t string_token);
}
