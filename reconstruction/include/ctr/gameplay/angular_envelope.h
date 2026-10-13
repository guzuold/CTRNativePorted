#pragma once
#include <array>
namespace ctr {
using AngularTuple = std::array<float, 3>;
// Original 00eff910: nullable tuple records +10/+20/+30, times +18/+1c/+28/+2c.
struct AngularEnvelopeView {
    const AngularTuple* initial;
    const AngularTuple* held;
    const AngularTuple* final;
    float initial_end, transition_duration, hold_duration, decay_duration;
};
AngularTuple evaluate_angular_envelope(const AngularEnvelopeView& envelope, float time);
}
