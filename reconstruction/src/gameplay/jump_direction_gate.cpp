// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/jump_direction_gate.h"
#include <bit>
#include <cmath>
#include <stdexcept>
namespace ctr {
namespace {
float norm2(Vec3 v){return ((v.x*v.x+0.f)+v.y*v.y)+v.z*v.z;}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Vec3 scale(Vec3 v,float f){return {f*v.x,f*v.y,f*v.z};}
}
bool jump_direction_gate(const JumpDirectionGateState& s,const GameplayHandle* handle,JumpGateDelayReader read,void* context,Vec3 axis,std::int32_t frame){
 if(!handle)throw std::invalid_argument("Missing required jump archetype handle");
 const auto* payload=gameplay_handle_payload(handle);
 Vec3 reference=s.reference;
 if(!s.reference_active){reference=axis;if(s.started!=-1){if(!payload||!read)throw std::invalid_argument("Missing original jump archetype delay query");
 const auto elapsed=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(frame)-static_cast<std::uint32_t>(s.started));
 if(read(payload,context)>=static_cast<float>(elapsed)*.0001220703125f)reference=s.reference;}}
 if(!(s.vertical_threshold>reference.z))return false;
 const float n=norm2(s.velocity);
 if(std::bit_cast<float>(std::uint32_t{0x350637bd})>=std::fabs(n))return false;
 const auto velocity=scale(s.velocity,1.f/std::sqrt(n));
 auto side=cross(reference,axis);side=scale(side,1.f/std::sqrt(norm2(side)+std::bit_cast<float>(std::uint32_t{0x00800000})));
 const auto plane=cross(side,reference);
 const float dot=((velocity.x*plane.x+0.f)+velocity.y*plane.y)+velocity.z*plane.z;
 return dot>s.direction_threshold;
}
}
