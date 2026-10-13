#include "ctr/gameplay/boost_interpolation.h"
#include <cmath>
#include <stdexcept>
namespace ctr {
void interpolate_boost_values(std::array<float, 6>& destination,
                              const std::array<float, 6>& source, float factor) {
    if (!std::isfinite(factor))
        throw std::invalid_argument("Nonfinite factor outside verified input contract");
    // Preserve arithmetic for full alias too; a direct assignment would change signed zero.
    for (unsigned i = 0; i < destination.size(); ++i)
        destination[i] = destination[i] + (source[i] - destination[i]) * factor;
}
}
