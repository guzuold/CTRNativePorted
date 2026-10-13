// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/jump_classification.h"
#include <stdexcept>
namespace ctr {
bool classify_jump(const JumpClassificationState& s,const EntityLocalView& e,Vec3 defaults,
 const GameplayHandle* handle,JumpClassificationReader reader,void* reader_context,
 const JumpClassificationServices& services,std::span<std::uint8_t,16> first,std::span<std::uint8_t,16> second){
 if(s.height_override>0.f)return true;
 if(!reader)throw std::invalid_argument("Missing original jump payload fields");
 const auto* p=gameplay_handle_payload(handle);
 if(!p)throw std::invalid_argument("Missing original jump archetype payload");
 const auto a=reader(p,reader_context);
 const float projection=air_velocity_projection(e,defaults,s.velocity);
 const auto* again=gameplay_handle_payload(handle);
 if(!again)throw std::invalid_argument("Missing original jump speed payload");
 const auto speed=reader(again,reader_context);
 if(!speed.speed_limit)throw std::invalid_argument("Missing original jump speed limit");
 const float ratio=projection/ *speed.speed_limit;
 // MINSS takes the second operand on equality or unordered inputs.
 const float capped=1.f<ratio?1.f:ratio;
 const float clamped=ratio<0.f?0.f:capped;
 if(!(clamped>a.ratio_threshold))return false;
 if(!services.position)throw std::invalid_argument("Missing original entity virtual+150 position");
 const auto pos=services.position(e,services.context);
 const Vec3 start{a.input_scale*s.input.x+pos.x,a.input_scale*s.input.y+pos.y,a.input_scale*s.input.z+pos.z};
 const Vec3 base{start.x+a.velocity_scale*s.velocity.x,start.y+a.velocity_scale*s.velocity.y,start.z+a.velocity_scale*s.velocity.z};
 if(!services.collision)throw std::invalid_argument("Missing original collision query00b6f310");
 if(services.collision(0x180,start,base,first,second,services.context))return false;
 if(!services.direction_gate)throw std::invalid_argument("Missing original jump direction gate adapter");
 const float distance=a.input_scale+(services.direction_gate(services.context)?a.gated_offset:a.ordinary_offset);
 const Vec3 endpoint{base.x-distance*s.input.x,base.y-distance*s.input.y,base.z-distance*s.input.z};
 return !services.collision(0x180,base,endpoint,first,second,services.context);
}
}
