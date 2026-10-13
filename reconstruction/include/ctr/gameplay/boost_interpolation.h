#pragma once
#include <array>
namespace ctr {
// Whole FUN_00ee6f00, six float32 fields at record +0xc through +0x20.
void interpolate_boost_values(std::array<float, 6>& destination,
                              const std::array<float, 6>& source, float factor);
}
