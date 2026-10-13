#pragma once
#include <cstdint>
#include <span>
namespace ctr {
// FUN_00b66300: 32-byte records, int32 key +0, vector +16.
void update_vector_record(std::span<std::uint8_t> records, std::int32_t count,
                          std::int32_t key, std::span<const std::uint8_t, 12> vector);
}
