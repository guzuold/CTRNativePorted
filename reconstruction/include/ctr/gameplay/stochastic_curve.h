// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "scalar_pattern.h"
namespace ctr {
struct CurveRandomState {std::uint32_t seed;float fraction;};
// Whole003f2480; borrowed84B curve, original256float random table.
float stochastic_curve(std::span<const std::uint8_t,84>,CurveRandomState&,float,bool,
 std::span<const float,256>,PatternNonlinear,void*);
}
