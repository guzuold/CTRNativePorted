#include "ctr/scalar_speed.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ctr {
SpeedResult advance_without_components(float speed, float dt, float steering,
                                       const LinearVelocityProfile& profile, SpeedMode mode) {
    if (!std::isfinite(speed) || !std::isfinite(dt) || !std::isfinite(steering) ||
        !std::isfinite(profile.maximum_speed) || profile.maximum_speed <= 0.0f || dt < 0.0f)
        throw std::invalid_argument("Invalid scalar speed inputs");
    const float ratio = speed / profile.maximum_speed;
    float step_factor = 1.0f;
    if (mode == SpeedMode::Brake && ratio > 1.0f)
        step_factor = (ratio - 1.0f) * 10.0f + 1.0f;
    const bool overspeed = ratio > 1.0499999523162842f; // DAT_01c9af30
    const float output_scale = overspeed ? speed : profile.maximum_speed;
    if (overspeed) mode = SpeedMode::Brake;
    const AccelerationData* data = nullptr;
    switch (mode) {
    case SpeedMode::Accelerate: data = profile.acceleration; break;
    case SpeedMode::Coast: data = profile.natural_deceleration; break;
    case SpeedMode::Brake: data = profile.braking; break;
    default: throw std::invalid_argument("Unknown speed mode");
    }
    if (!data) throw std::invalid_argument("Missing selected acceleration data");
    const float normalized = std::clamp(ratio, 0.0f, 1.0f);
    const float inverse_time = data->curve.inverse(normalized, 10);
    float duration = data->duration;
    if (steering != 0.0f && data->turning_duration > 0.0f) {
        const float magnitude = std::fabs(steering);
        duration = duration * (1.0f - magnitude) + data->turning_duration * magnitude;
    }
    if (!std::isfinite(duration) || duration <= 0.0f)
        throw std::invalid_argument("Invalid selected duration");
    const float time = std::clamp(inverse_time + (step_factor * dt) / duration, 0.0f, 1.0f);
    return {data->curve.evaluate(time) * output_scale, inverse_time};
}
}
