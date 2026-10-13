// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "stochastic_curve.h"
namespace ctr {
struct JumpScalePayload {
 float base_speed;
 std::span<const std::uint8_t,84> ordinary_curve,positive_curve;
 const float* speed_limit;
};
struct JumpScaleServices {
 // Must implement the actual whole handle lookup, including mutable/alias paths.
 JumpScalePayload (*payload)(bool second_lookup,void*);
 float (*orientation_y)(void*);
 float (*sine)(float,void*);
 // Genuine fallback only outside verified QuantizedCurve deterministic domain.
 float (*ordinary_curve)(std::span<const std::uint8_t,84>,float,void*);
 float (*air_projection)(void*);
 PatternNonlinear nonlinear;
 void* context;
};
// Whole00efeee0, mandatory original service boundaries; pending original comparison.
float jump_scale(const JumpScaleServices&,std::span<const float,256> original_random_table);
}
