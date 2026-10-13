#pragma once
#include <array>
#include <cstdint>
namespace ctr {
using OrientationBits = std::array<std::uint32_t,4>;
// Whole004d9f10 borrowed unparented domain, no quaternion normalization.
OrientationBits local_orientation(const OrientationBits* transform, bool has_parent,
                                  OrientationBits original_default);
}
