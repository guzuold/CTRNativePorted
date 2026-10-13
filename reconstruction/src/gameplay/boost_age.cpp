#include "ctr/gameplay/boost_age.h"
#include <bit>
#include <cmath>
#include <stdexcept>
namespace ctr {
float boost_age(bool active, std::int32_t now, std::int32_t started,
                std::int32_t finished, float delay) {
    if (!active && finished == -1) return 0.f;
    if (!std::isfinite(delay)) throw std::invalid_argument("Nonfinite boost delay outside verified contract");
    const auto end = active ? now : finished;
    const auto delta = std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(end) - static_cast<std::uint32_t>(started));
    const float age = static_cast<float>(delta) * 0.0001220703125f - delay;
    // The original clamps only the active branch, preserving negative finished ages.
    return active && age <= 0.f ? 0.f : age;
}
}
