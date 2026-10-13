// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "ctr/gameplay/air_velocity_projection.h"
#include "ctr/gameplay/handle_payload.h"
#include <span>
namespace ctr {
struct JumpClassificationState { float height_override; Vec3 input,velocity; };
struct JumpClassificationPayload { float ratio_threshold,velocity_scale,input_scale,ordinary_offset,gated_offset; const float* speed_limit; };
using JumpClassificationReader=JumpClassificationPayload(*)(const GameplayPayload*,void*);
struct JumpClassificationServices {
 Vec3 (*position)(const EntityLocalView&,void*);
 bool (*collision)(unsigned,Vec3 start,Vec3 end,std::span<std::uint8_t,16>,std::span<std::uint8_t,16>,void*);
 bool (*direction_gate)(void*);
 void* context;
};
// Whole00efe9c0. Borrowed views; query/virtual callbacks require genuine services.
bool classify_jump(const JumpClassificationState&,const EntityLocalView&,Vec3 defaults,
 const GameplayHandle*,JumpClassificationReader,void*,const JumpClassificationServices&,
 std::span<std::uint8_t,16> query_first,std::span<std::uint8_t,16> query_second);
}
