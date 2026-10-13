// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/contact_sample.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace {
template<class T>T read(const std::byte*p){T v;std::memcpy(&v,p,sizeof(v));return v;}
const std::byte* pointer(const std::byte*p){return reinterpret_cast<const std::byte*>(read<std::uintptr_t>(p));}
std::int32_t bound(const std::byte*keys,std::int32_t count,float value,bool upper){
 std::int32_t first=0;
 while(count>0){const auto half=count/2;const auto middle=first+half;const auto key=read<float>(keys+middle*4);
  const bool advance=upper ? !(key>value) : value>key;
  if(advance){first=middle+1;count-=half+1;}else count=half;
 }return first;
}
}
namespace ctr {
std::uint32_t contact_sample(std::span<const std::byte,112> geometry,float parameter,std::span<float,3> output){
 const auto*g=geometry.data();const auto range=read<float>(g+104);const bool wrap=read<unsigned char>(g+108)!=0;
 if(!std::isfinite(parameter)||!std::isfinite(range)||range<=0)throw std::runtime_error("Missing nonfinite/nonpositive range contract");
 if(wrap){if(parameter<0)parameter=range-std::fmod(0.0f-parameter,range);else if(parameter>range)parameter=std::fmod(parameter,range)+0.0f;}
 else {float selected=parameter>range?range:parameter;parameter=parameter<0?0.0f:selected;}
 const auto count=read<std::int32_t>(g+24);if(count<0)throw std::runtime_error("Missing negative geometry count contract");
 const auto*keys=pointer(g+40);const auto index=bound(keys,count,parameter,false);
 if(index==count)return 1;
 const auto*records=pointer(g+64);const auto*c=records+static_cast<std::size_t>(index)*136;
 // The original performs a second lower-bound read of the selected outer key.
 (void)bound(keys,count,read<float>(keys+index*4),false);
 const auto*lengths=pointer(c+16);float local=0;
 if(!(read<float>(lengths)>=parameter)){
  const auto samples=read<std::int32_t>(c);if(samples<0)throw std::runtime_error("Missing negative sample count contract");
  const auto sample=bound(lengths,samples,parameter,true);
  if(sample==samples)local=1.0f;
  else {if(sample==0)throw std::runtime_error("Missing invalid length ordering");const auto*params=pointer(c+40);
   const float first=read<float>(lengths+(sample-1)*4),last=read<float>(lengths+sample*4),pfirst=read<float>(params+(sample-1)*4),plast=read<float>(params+sample*4);
   const float delta=parameter-first;const float length=last-first;const float pdiff=plast-pfirst;const float scale=pdiff/length;const float scaled=delta*scale;local=pfirst+scaled;
  }
 }
 const float inverse=1.0f-local;const float triple=local*3.0f;const float square=local*local;
 const float temp=triple*inverse;const float invsquare=inverse*inverse;const float localscale=local*triple;
 const float w3=local*square,w0=inverse*invsquare,w1=inverse*temp,w2=inverse*localscale;
 for(std::size_t axis=0;axis<3;++axis){const float a=w0*read<float>(c+80+axis*4),b=w1*read<float>(c+92+axis*4),cc=w2*read<float>(c+104+axis*4),d=w3*read<float>(c+116+axis*4);const float ab=a+b;const float abc=ab+cc;output[axis]=abc+d;}
 return 0;
}
}
