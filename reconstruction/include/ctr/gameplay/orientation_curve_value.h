// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "stochastic_curve.h"
namespace ctr {
// Retail payload fields +170/+174 and curves +17c/+1d0. Names chosen here.
struct OrientationCurvePayload {
 float scale,bonus;
 std::span<const std::uint8_t,84> ordinary_curve,positive_curve;
};
struct OrientationCurveServices {
 // Must perform the actual immutable/alias/mutable lookup of controller+b8.
 OrientationCurvePayload (*payload)(void*);
 float (*orientation_y)(void*);
 float (*sine)(float,void*);
 // Genuine fallback only outside verified QuantizedCurve deterministic domain.
 float (*ordinary_curve)(std::span<const std::uint8_t,84>,float,void*);
 PatternNonlinear nonlinear;
 void* context;
};
// Whole00eeb9a0; compiled reconstruction, original comparison still required.
float orientation_curve_value(const OrientationCurveServices&,bool add_bonus,
                              std::span<const float,256> original_random_table);
}
