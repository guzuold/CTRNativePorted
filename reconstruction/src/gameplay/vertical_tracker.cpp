#include "ctr/gameplay/vertical_tracker.h"
#include <bit>
#include <cmath>
#include <stdexcept>
namespace ctr {
void update_vertical_tracker(VerticalTracker& state, const EntityLocalView& entity,
                             float elapsed, float rate) {
    if (!std::isfinite(state.reference_height) || !std::isfinite(state.accumulated) ||
        !std::isfinite(elapsed) || !std::isfinite(rate))
        throw std::invalid_argument("Non-finite vertical tracker domain");
    constexpr float sentinel = std::bit_cast<float>(0xff7fffffu);
    if (state.reference_height == sentinel) { state.active = 0; return; }
    const float height = unparented_position(entity).z;
    if (!std::isfinite(height)) throw std::invalid_argument("Non-finite height domain");
    const float delta = height - state.reference_height;
    if (!std::isfinite(delta)) throw std::invalid_argument("Non-finite height delta domain");
    if (delta > 0.0f) {
        if (state.active) state.accumulated = 0.0f;
        else { state.reference_height = sentinel; state.accumulated = 0.0f; }
    } else if (!state.active && !(delta < -80.0f)) {
        state.reference_height = sentinel; state.accumulated = 0.0f;
    } else {
        const float negated = std::bit_cast<float>(std::bit_cast<std::uint32_t>(delta) ^ 0x80000000u);
        // MINSS selects operand two on equal inputs, including signed zeros.
        const float selected = state.accumulated < negated ? state.accumulated : negated;
        const float increment = elapsed * rate;
        state.accumulated = selected + increment;
    }
    state.active = 0;
}
}
