#include "ctr/core/contact_curve_search.h"
#include "ctr/core/contact_curve_query.h"
#include <array>
#include <bit>
#include <cstring>
#include <immintrin.h>
#include <stdexcept>
namespace {
template<class T>T read(const std::byte*p){T v{};std::memcpy(&v,p,sizeof(v));return v;}
float mul(float a,float b){return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a),_mm_set_ss(b)));}
float add(float a,float b){return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a),_mm_set_ss(b)));}
float sub(float a,float b){return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a),_mm_set_ss(b)));}
// 13d3a99..13d3b41 and13d3c81..13d3d29; preserve original arithmetic order.
std::array<float,3> evaluate(const std::byte*r,float t){
 const float omt=sub(1.f,t),three_t=mul(t,3.f),tt=mul(t,t),oo=mul(omt,omt);
 const float mid=mul(three_t,omt),second=mul(t,three_t),first=mul(omt,oo);
 const float a=mul(omt,mid),b=mul(omt,second),c=mul(t,tt);
 std::array<float,3> v{};
 for(std::size_t k=0;k<3;++k){
  const float p0=mul(first,read<float>(r+0x50+k*4));
  const float p1=mul(a,read<float>(r+0x5c+k*4));
  const float p2=mul(b,read<float>(r+0x68+k*4));
  const float p3=mul(c,read<float>(r+0x74+k*4));
  v[k]=add(add(add(p0,p1),p2),p3);
 }return v;
}
}
namespace ctr {
float contact_curve_search(std::span<const std::byte,128> receiver,std::span<const float,3> query,float& squared_distance,std::int32_t axis,const std::uint64_t& guard){
 if(axis < -1 || axis>2)throw std::runtime_error("Missing out-of-range axis contract");
 const auto*r=receiver.data();const auto saved_guard=guard;
 auto previous=std::array<float,3>{read<float>(r+0x38),read<float>(r+0x3c),read<float>(r+0x40)};
 if(axis!=-1)previous[static_cast<std::size_t>(axis)]=0.f;
 float best=std::bit_cast<float>(std::uint32_t{0x7f7fffff}),position=0.f;
 const std::int32_t count=read<std::int32_t>(r);
 if(count>=2){
  const auto*parameters=reinterpret_cast<const float*>(read<std::uintptr_t>(r+0x28));
  const auto*lengths=reinterpret_cast<const float*>(read<std::uintptr_t>(r+0x10));
  const auto*cache=reinterpret_cast<const std::byte*>(read<std::uintptr_t>(r+0x30));
  if(!parameters||!lengths)throw std::runtime_error("Missing borrowed curve tables");
  std::array<float,32> control{};std::memcpy(control.data(),r,128);
  for(std::int32_t i=1;i<count;++i){
   std::array<float,3> endpoint{};
   if(cache){const auto*points=reinterpret_cast<const std::byte*>(read<std::uintptr_t>(cache+0x20));if(!points)throw std::runtime_error("Missing cached points backing");std::memcpy(endpoint.data(),points+static_cast<std::size_t>(i)*12,12);}
   else endpoint=evaluate(r,parameters[i]);
   if(axis!=-1)endpoint[static_cast<std::size_t>(axis)]=0.f;
   float projected=0.f;
   const float distance=contact_curve_query(control,previous,endpoint,query,parameters[i-1],parameters[i],axis,projected,guard);
   if(best>distance){best=distance;position=add(lengths[i-1],projected);}
   previous=endpoint;
  }
 }
 squared_distance=best;
 if(saved_guard!=guard)throw std::runtime_error("Missing original stack guard failure1adb0d0");
 return position;
}
}
