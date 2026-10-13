// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/orientation_curve_value.h"
#include "ctr/gameplay/constant_curve.h"
#include <bit>
#include "ctr/scalar_speed.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
float orientation_curve_value(const OrientationCurveServices& services,bool add_bonus,
                              std::span<const float,256> table){
 if(!services.payload||!services.orientation_y||!services.sine)
  throw std::invalid_argument("Missing original payload/orientation/sine service");
 const auto payload=services.payload(services.context);
 const float angle=services.orientation_y(services.context);
 const float sine=services.sine(angle,services.context);
 const float selected=angle!=0.f?sine:0.f;
 const auto curve=selected>0.f?payload.positive_curve:payload.ordinary_curve;
 const float input=std::bit_cast<float>(std::bit_cast<std::uint32_t>(selected)&0x7fffffffu);
 CurveRandomState state{0x41a7u,1.f};
 std::uint16_t flags;std::memcpy(&flags,curve.data()+80,2);
 float value;
 if(flags&24u)value=stochastic_curve(curve,state,input,false,table,services.nonlinear,services.context);
 else if((flags&7u)==1u){
  std::array<std::uint8_t,7> key;std::memcpy(key.data(),curve.data(),key.size());
  value=constant_curve(key);
 }else{
  // Reuse whole003f22b0 only within its verified deterministic domain.
  std::array<std::uint8_t,84> bytes;std::memcpy(bytes.data(),curve.data(),bytes.size());
  try { value=QuantizedCurve(bytes).evaluate(input); }
  catch(const std::invalid_argument&) {
   // Unsupported layouts/inputs still require a genuine whole original service.
   if(!services.ordinary_curve)throw;
   value=services.ordinary_curve(curve,input,services.context);
  }
 }
 const float scaled=value*payload.scale;
 return add_bonus?scaled+payload.bonus:scaled;
}
}
