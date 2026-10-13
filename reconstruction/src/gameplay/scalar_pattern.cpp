// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/scalar_pattern.h"
#include <bit>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace ctr {
float scalar_pattern(std::span<const std::uint8_t,20> p,std::uint32_t& seed,float x,float phase,
 std::span<const float,256> table,PatternNonlinear nonlinear,void* context){
 auto f=[&](unsigned o){float v;std::memcpy(&v,p.data()+o,4);return v;};
 const unsigned flags=p[19],kind=flags&15,mode=(flags>>4)&3;
 if(kind<1||kind>8)return 0.f;
 float value=0.f;const float offset=(flags&128)?phase:0.f;
 if(kind<=3){
  seed*=0x41a7u;const unsigned base=(seed>>12)&255;
  std::uint16_t period;std::memcpy(&period,p.data()+16,2);
  const float t=offset+x*f(12);float reduced=t;
  if(!std::isfinite(t))throw std::invalid_argument("Missing nonfinite original pattern domain");
  while(reduced>=256.f)reduced+=-256.f;
  unsigned index=0;while(reduced>=32.f){reduced+=-32.f;index+=32;}
  while(reduced>=1.f){reduced+=-1.f;++index;}
  if(index>=period)index-=period;
  const unsigned first=(base+index)&255;
  value=table[first];
  if(kind>=2){
   const float u=t-std::floor(t);
   const unsigned limit=base+period;
   unsigned next=first+1;if(next>=limit)next=base;
   const float b=table[next&255];
   if(kind==2)value=value+u*(b-value);
   else {
    unsigned third=next+1;if(third>=limit)third=base;
    unsigned fourth=third+1;if(fourth>=limit)fourth=base;
    const float c=table[third&255],d=table[fourth&255];
    const float u2=u*u,u3=u*u2;
    const float a0=(((u2+u2)-u3)-u)*value;
    const float a1=((u3*3.f+u2*-5.f)+2.f)*b;
    const float a2=(u+(u2*4.f-u3*3.f))*c;
    const float a3=(u3-u2)*d;
    value=((a0+a1)+a2)+a3;value*=.5f;
   }
  }
 }else if(kind==4){value=x*1759.f+phase;value=value+(x*phase)*7919.f;value=value-std::floor(value);}
 else {
  float t=(kind==5&&!(flags&128))?.75f:offset;
  t=(t+x*f(12))+f(4);t=t-std::floor(t);
  if(kind==5)value=std::sin(t*std::bit_cast<float>(std::uint32_t{0x40c90fdb}))*.5f+.5f;
  else if(kind==6)value=t;
  else if(kind==7)value=1.f-t;
  else {value=t+t;if(!(t<.5f))value=2.f-value;}
 }
 if(mode==2){if(!nonlinear)throw std::invalid_argument("Missing original nonlinear014d1230");value=nonlinear(value,context)*.5f;value=value*f(8)+.5f;const float cap=1.f<value?1.f:value;return value<0.f?0.f:cap;}
 if(mode==1)return f(8)<=value?1.f:0.f;
 return value;
}
}

