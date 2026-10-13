// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <array>
#include <cstdint>
#include <span>
namespace ctr {
using PatternNonlinear=float(*)(float,void*);
// Whole retail003f18c0, borrowed pattern/table; pending original verification.
float scalar_pattern(std::span<const std::uint8_t,20>,std::uint32_t& seed,float input,float phase,
 std::span<const float,256> table,PatternNonlinear,void*);
}
