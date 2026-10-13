// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "ctr/gameplay/handle_payload.h"
#include <cstdint>
namespace ctr {
// Decoded borrowed payload+0xa8 -> limits; thresholds at limits+18/+1c.
struct ThresholdSelectionPayload : GameplayPayload { float first; float second; };
// Whole 00ee9420. Only state3/4 and mode1/2 access the mandatory payload.
std::uint32_t threshold_selection(std::uint32_t state,std::uint32_t mode,
                                 float elapsed,const GameplayHandle* handle);
}
