// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/jump_scale.h"
#include <bit>
#include "ctr/scalar_speed.h"
#include "ctr/gameplay/constant_curve.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
float jump_scale(const JumpScaleServices& services,std::span<const float,256> table){
 if(!services.payload||!services.orientation_y||!services.sine)
  throw std::invalid_argument("Missing jump scale payload/orientation/sine service");
 const auto first=services.payload(false,services.context);
 const float angle=services.orientation_y(services.context);
 const float sine=services.sine(angle,services.context);
 const float selected_sine=angle!=0.f?sine:0.f;
 const auto curve=selected_sine>0.f?first.positive_curve:first.ordinary_curve;
 const float input=std::bit_cast<float>(std::bit_cast<std::uint32_t>(selected_sine)&0x7fffffffu);
 CurveRandomState state{0x41a7u,1.f};
 std::uint16_t flags;std::memcpy(&flags,curve.data()+80,2);
 float value;
 if(flags&24) value=stochastic_curve(curve,state,input,false,table,services.nonlinear,services.context);
 else if((flags&7)==1){
  std::array<std::uint8_t,7> key;std::memcpy(key.data(),curve.data(),key.size());
  value=constant_curve(key);
 }else {
  // Reuse whole003f22b0 only within its verified deterministic domain.
  std::array<std::uint8_t,84> bytes;std::memcpy(bytes.data(),curve.data(),bytes.size());
  try { value=QuantizedCurve(bytes).evaluate(input); }
  catch(const std::invalid_argument&) {
   // Unsupported layouts/inputs still require a genuine whole original service.
   if(!services.ordinary_curve)throw;
   value=services.ordinary_curve(curve,input,services.context);
  }
 }
 float factor=1.f;
 if(value!=1.f){
  if(!services.air_projection)throw std::invalid_argument("Missing jump scale air projection service");
  const float projection=services.air_projection(services.context);
  const auto second=services.payload(true,services.context);
  if(!second.speed_limit)throw std::invalid_argument("Missing jump scale speed limit object");
  const float ratio=projection / *second.speed_limit;
  // MINSS selects its second operand on unordered comparison.
  const float clamped=ratio<0.f?0.f:(1.f<ratio?1.f:ratio);
  factor=1.f+(value+-1.f)*clamped;
 }
 return factor*first.base_speed;
}
}
