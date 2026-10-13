// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "ctr/gameplay/handle_payload.h"
#include <cstdint>
namespace ctr {
// Borrowed decoded retail payload: kind at +0x5c for first handle,
// selection at +0x40 for second. Names chosen by this project.
struct ActivityGatePayload : GameplayPayload { std::int32_t kind; std::int32_t selection; };
// Whole 00ee96c0. Mutable handles require the missing original resolver.
bool activity_gate(const GameplayHandle* first, const GameplayHandle* second,
                   std::uint32_t input_a, std::uint32_t input_b);
}
