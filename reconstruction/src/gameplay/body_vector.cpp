#include "ctr/gameplay/body_vector.h"
#include <cmath>
#include <cstring>
#include "ctr/gameplay/body_record.h"
#include <stdexcept>
namespace ctr {
std::array<float,4> transform_body_vector(std::array<float,4> q,std::array<float,4> v){
 for(unsigned i=0;i<4;++i)if(!std::isfinite(q[i])||!std::isfinite(v[i]))throw std::invalid_argument("Nonfinite body vector input");
 const float p0=q[0]*v[0],p1=q[1]*v[1],p2=q[2]*v[2];
 const float dot=(p0+p1)+(p2+0.f);
 const float square=q[3]*q[3];const float factor=square-0.5f;
 constexpr unsigned perm[4]={1,2,0,3};
 std::array<float,4> cross{},out{};
 for(unsigned i=0;i<4;++i){const float a=v[perm[i]]*q[i];const float b=v[i]*q[perm[i]];cross[i]=a-b;}
 for(unsigned i=0;i<4;++i){
  const float a=dot*q[i];const float b=cross[perm[i]]*q[3];const float c=factor*v[i];
  const float sum=a+b;const float sum2=c+sum;out[i]=sum2+sum2;
 }
 return out;
}
std::array<float,4> body_vector(std::span<const std::byte> mapping,std::int32_t index,std::span<const std::byte> bodies){
 const auto base=body_record_offset(mapping,index,bodies.size())-64;
 std::array<float,4> q,v;std::memcpy(q.data(),bodies.data()+base+16,16);std::memcpy(v.data(),bodies.data()+base+80,16);
 return transform_body_vector(q,v);
}
}
