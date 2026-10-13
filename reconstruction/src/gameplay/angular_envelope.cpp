#include "ctr/gameplay/angular_envelope.h"
#include <cmath>
#include <stdexcept>
namespace ctr {
namespace {
constexpr AngularTuple defaults{0.f, 1.f, 1.f};
AngularTuple sample(const AngularTuple* tuple) { return tuple ? *tuple : defaults; }
AngularTuple interpolate(const AngularTuple* a, const AngularTuple* b,
                         float time, float start, float end) {
    if (!a || !b) return defaults;
    const float fraction = (time - start) / (end - start);
    return {(*a)[0] + fraction * ((*b)[0] - (*a)[0]),
            (*a)[1] + fraction * ((*b)[1] - (*a)[1]),
            (*a)[2] + fraction * ((*b)[2] - (*a)[2])};
}
}
AngularTuple evaluate_angular_envelope(const AngularEnvelopeView& e, float time) {
    if (!std::isfinite(time) || !std::isfinite(e.initial_end) ||
        !std::isfinite(e.transition_duration) || !std::isfinite(e.hold_duration) ||
        !std::isfinite(e.decay_duration))
        throw std::invalid_argument("Nonfinite envelope timing outside verified contract");
    float start = e.initial_end;
    if (time <= start) return sample(e.initial);
    float end = start + e.transition_duration;
    if (time < end) return interpolate(e.initial, e.held, time, start, end);
    start = end + e.hold_duration;
    if (time <= start) return sample(e.held);
    end = start + e.decay_duration;
    if (time < end) return interpolate(e.held, e.final, time, start, end);
    return sample(e.final);
}
}
