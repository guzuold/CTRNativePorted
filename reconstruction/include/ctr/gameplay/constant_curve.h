#pragma once
#include <array>
#include <cstdint>
namespace ctr {
// Whole FUN_003f2440. Scale at +0; signed quantized value at +6.
float constant_curve(const std::array<std::uint8_t, 7>& key);
}
