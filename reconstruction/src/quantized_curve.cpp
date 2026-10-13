#include "ctr/scalar_speed.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ctr {
QuantizedCurve::QuantizedCurve(const std::array<std::uint8_t, 84>& bytes) : bytes_(bytes) {
    const unsigned flags = bytes_[0x50] | (unsigned(bytes_[0x51]) << 8);
    count_ = flags & 7;
    if ((flags & 0x18) != 0 || count_ < 2 || count_ > 6)
        throw std::invalid_argument("Unsupported curve: stochastic/constant or invalid key count");
    // Seven 12-byte keys would overlap the metadata at +0x50.
    for (int i = 0; i < count_; ++i) {
        if (!std::isfinite(scale_at(i)) || (i && time_at(i) <= time_at(i - 1)))
            throw std::invalid_argument("Invalid quantized curve keys");
    }
}
float QuantizedCurve::signed_byte(int offset) const {
    const int value = bytes_.at(offset);
    return static_cast<float>(value < 128 ? value : value - 256);
}
float QuantizedCurve::scale_at(int index) const {
    float value;
    std::memcpy(&value, bytes_.data() + index * 12, sizeof(value));
    return value;
}
float QuantizedCurve::time_at(int index) const {
    // DAT_01c05908 = 0x3c010204 (float32 1/127).
    return signed_byte(index * 12 + 5) * 0.007874015718698502f;
}
float QuantizedCurve::value_at(int index) const {
    return signed_byte(index * 12 + 6) * scale_at(index);
}
float QuantizedCurve::evaluate(float time) const {
    if (!std::isfinite(time)) throw std::invalid_argument("Non-finite curve time");
    float left_time = time_at(0);
    if (time < left_time) return value_at(0);
    if (time_at(count_ - 1) <= time) return value_at(count_ - 1);
    int index = 0;
    while (index < count_ - 2 && time > time_at(index + 1)) ++index;
    left_time = time_at(index);
    const float left_value = value_at(index);
    const float right_value = value_at(index + 1);
    // Preserve original operation order and FLT_MIN denominator adjustment.
    const float fraction = (time - left_time) /
        ((time_at(index + 1) - left_time) + std::numeric_limits<float>::min());
    if (bytes_[index * 12 + 4] != 0)
        return left_value + fraction * (right_value - left_value);
    const float scale = scale_at(index);
    const float tangent0 = (scale * signed_byte(index * 12 + 8) - left_value) * 3.0f;
    const float tangent1 = (scale * signed_byte(index * 12 + 9) -
                           scale * signed_byte(index * 12 + 8)) * 3.0f - tangent0;
    return left_value + fraction * (tangent0 + fraction *
        (tangent1 + fraction * (right_value - (left_value + tangent0 + tangent1))));
}
float QuantizedCurve::inverse(float value, int iterations) const {
    if (!std::isfinite(value) || iterations < 1 || iterations > 64)
        throw std::invalid_argument("Invalid inverse arguments");
    float lower_value = evaluate(0.0f), upper_value = evaluate(1.0f);
    float lower_time = 0.0f, upper_time = 1.0f;
    const float fallback = -1.0f; // Original out-of-range/not-invertible sentinel.
    if (lower_value > upper_value) {
        std::swap(lower_value, upper_value);
        std::swap(lower_time, upper_time);
    }
    constexpr float tolerance = 4.999999987376214e-7f; // DAT_01c97c38
    if (std::fabs(value - lower_value) <= tolerance) return lower_time;
    if (value < lower_value || value > upper_value)
        return std::fabs(value - upper_value) <= tolerance ? upper_time : fallback;
    if (std::fabs(value - upper_value) <= tolerance) return upper_time;
    float middle = 0.5f;
    for (int i = 0; i < iterations; ++i) {
        const float sampled = evaluate(middle);
        if (sampled < lower_value || sampled > upper_value) return fallback;
        if (std::fabs(value - sampled) <= tolerance) return middle;
        if (sampled > value) { upper_time = middle; upper_value = sampled; }
        else { lower_time = middle; lower_value = sampled; }
        middle = (lower_time + upper_time) * 0.5f;
    }
    return middle;
}
}
