#include "ctr/gameplay/constant_curve.h"
#include <cstring>
namespace ctr {
float constant_curve(const std::array<std::uint8_t, 7>& key) {
    float scale;
    std::memcpy(&scale, key.data(), sizeof(scale));
    const int value = key[6] < 128 ? key[6] : int(key[6]) - 256;
    return static_cast<float>(value) * scale;
}
}
