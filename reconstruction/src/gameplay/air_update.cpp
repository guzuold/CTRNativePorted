// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/air_update.h"
#include "ctr/gameplay/air_velocity_projection.h"
#include "ctr/gameplay/vector_record.h"
#include <bit>
#include <cmath>
#include <stdexcept>
namespace ctr {
namespace {
float bits(std::uint32_t x){return std::bit_cast<float>(x);}
float neg(float x){return bits(std::bit_cast<std::uint32_t>(x)^0x80000000u);}
float dot(Vec3 a,Vec3 b){return ((a.x*b.x+0.f)+a.y*b.y)+a.z*b.z;}
Vec3 normalize(Vec3 v){const float r=1.f/std::sqrt(dot(v,v)+bits(0x00800000));return {r*v.x,r*v.y,r*v.z};}
Vec3 subtract(Vec3 a,float f,Vec3 b){return {a.x-f*b.x,a.y-f*b.y,a.z-f*b.z};}
}
void update_air_direction(AirUpdateState& s,const EntityLocalView& entity,Vec3 defaults,Vec3 velocity,
 const GameplayHandle* optional_handle,const GameplayHandle* required_handle,
 AirPayloadReader reader,void* context,std::span<std::uint8_t> records,std::int32_t count){
 Vec3 emitted{};
 if(!s.active){s.direction={0.f,0.f,-1.f};emitted=s.direction;}
 else {
  bool invert=s.override_input||s.input.z>bits(0x3f7c28f6);
  if(!invert){const auto* p=gameplay_handle_payload(optional_handle);if(p){if(!reader)throw std::invalid_argument("Missing original air payload type query");invert=reader(p,context).type_id!=-1;}}
  if(invert)s.direction={neg(s.input.x),neg(s.input.y),neg(s.input.z)};
  else if(s.steer_a||s.steer_b){const float k=bits(0xbf4ccccd);s.direction=normalize({k*s.input.x,k*s.input.y,k*s.input.z+bits(0xbe4ccccc)});}
  else {
   s.direction={0.f,0.f,-1.f};const auto side=entity_side(entity,defaults);
   const auto a=unparented_angles(entity,defaults);
   const float cy=a.y==0.f?1.f:std::cos(a.y),sz=a.z==0.f?0.f:std::sin(a.z),cz=a.z==0.f?1.f:std::cos(a.z);
   const float nsy=a.y==0.f?-0.f:neg(std::sin(a.y));
   const auto before=s.direction;const float projection=dot(side,before);
   const bool large=std::fabs(projection)>=.5f;
   if(!large)s.direction=subtract(s.direction,projection,side);
   (void)air_velocity_projection(entity,defaults,velocity);
   // Both original handle traversals retained; they may resolve mutable records.
   (void)gameplay_handle_payload(required_handle);
   const float speed=air_velocity_projection(entity,defaults,velocity);
   const auto* p=gameplay_handle_payload(required_handle);
   if(!p||!reader)throw std::invalid_argument("Missing required air archetype payload query");
   const auto config=reader(p,context);if(!config.speed_limit)throw std::invalid_argument("Missing required air speed limit object+0xc");
   const float ratio=speed / *config.speed_limit;
   const float clamped=ratio<0.f?0.f:std::fmin(1.f,ratio);
   if(!(std::fabs(clamped)<=std::fabs(nsy))){const Vec3 horizontal{cy*cz,cy*sz,nsy};const float amount=dot(before,horizontal);s.direction=subtract(s.direction,amount,horizontal);}
   else if(large){emitted=s.direction;goto emit;}
   s.direction=normalize(s.direction);
  }
  emitted=s.direction;
 }
 emit:
 const Vec3 scaled{s.output_scale*emitted.x,s.output_scale*emitted.y,s.output_scale*emitted.z};
 const auto* raw=reinterpret_cast<const std::uint8_t*>(&scaled);
 update_vector_record(records,count,s.record_key,std::span<const std::uint8_t,12>(raw,12));
}
}
