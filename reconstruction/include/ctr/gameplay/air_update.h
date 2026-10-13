// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "ctr/direction.h"
#include "ctr/gameplay/handle_payload.h"
#include <span>
namespace ctr {
struct AirUpdateState { bool active, override_input, steer_a, steer_b; Vec3 input, direction; float output_scale; std::int32_t record_key; };
struct AirPayloadData { std::int32_t type_id; const float* speed_limit; };
using AirPayloadReader=AirPayloadData(*)(const GameplayPayload*,void*);
// Borrowed views, not retail object layout. Whole00efd7e0; pending original verification.
void update_air_direction(AirUpdateState&,const EntityLocalView&,Vec3 default_angles,Vec3 velocity,
 const GameplayHandle* optional_handle,const GameplayHandle* required_handle,
 AirPayloadReader,void* reader_context,std::span<std::uint8_t> records,std::int32_t count);
}
