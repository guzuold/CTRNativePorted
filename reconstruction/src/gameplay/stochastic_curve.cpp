// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/stochastic_curve.h"
#include <bit>
#include <cstring>
#include <stdexcept>
namespace ctr {
float stochastic_curve(std::span<const std::uint8_t,84> curve,CurveRandomState& state,float x,bool reverse,
 std::span<const float,256> table,PatternNonlinear nonlinear,void* context){
 auto f=[&](unsigned o){float v;std::memcpy(&v,curve.data()+o,4);return v;};
 auto s=[&](unsigned o){return float(static_cast<std::int8_t>(curve[o]));};
 std::uint16_t flags;std::memcpy(&flags,curve.data()+80,2);const unsigned count=flags&7;
 if(count<1||count>5)throw std::invalid_argument("Missing invalid stochastic curve layout");
 const float phase=state.fraction+-1.f;
 state.seed*=0x41a7u;state.fraction=std::bit_cast<float>((state.seed&0x7fffffu)|0x3f800000u);
 const auto pattern=std::span<const std::uint8_t,20>(curve.data()+60,20);
 const float value=scalar_pattern(pattern,state.seed,x,phase,table,nonlinear,context);
 auto endpoint=[&](unsigned i){const unsigned o=i*12;const float a=f(o)*s(o+6),b=f(o)*s(o+7);
  if(!(curve[79]&15))return a+phase*b;
  if(curve[79]&64){const float base=a+phase*b;return base+(value+-.5f)*f(60);}
  const float blend=phase+(value-phase)*f(60);return a+b*blend;
 };
 float result;
 const float time_scale=std::bit_cast<float>(std::uint32_t(0x3c010204));
 if(count==1||x<s(5)*time_scale)result=endpoint(0);
 else if(x>=s((count-1)*12+5)*time_scale)result=endpoint(count-1);
 else {
  unsigned i=0;while(i<count-2&&x>s((i+1)*12+5)*time_scale)++i;
  const unsigned o=i*12,n=o+12;const float a=endpoint(i),b=endpoint(i+1);
  const float t=(x-s(o+5)*time_scale)/((s(n+5)*time_scale-s(o+5)*time_scale)+std::bit_cast<float>(std::uint32_t(0x00800000)));
  if(curve[o+4])result=a+t*(b-a);
  else {
   const float p=(f(o)*s(o+7))*phase+f(o)*s(o+8);
   const float q=(f(n)*s(n+7))*phase+f(o)*s(o+9);
   const float first=(p-a)*3.f;
   const float middle=(q-p)*3.f-first;
   const float last=b-((a+first)+middle);
   result=a+t*(first+t*(middle+t*last));
  }
 }
 if((flags&16)&&reverse)result=std::bit_cast<float>(std::bit_cast<std::uint32_t>(result)^0x80000000u);
 return result;
}
}
