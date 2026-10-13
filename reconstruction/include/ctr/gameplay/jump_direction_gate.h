// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "ctr/object_access.h"
#include "ctr/gameplay/handle_payload.h"
namespace ctr {
struct JumpDirectionGateState {bool reference_active;std::int32_t started;Vec3 reference,velocity;float vertical_threshold,direction_threshold;};
using JumpGateDelayReader=float(*)(const GameplayPayload*,void*);
// Whole00efecc0 recovered; explicit borrowed original global vector/frame.
bool jump_direction_gate(const JumpDirectionGateState&,const GameplayHandle*,JumpGateDelayReader,void*,Vec3 original_axis,std::int32_t frame);
}
