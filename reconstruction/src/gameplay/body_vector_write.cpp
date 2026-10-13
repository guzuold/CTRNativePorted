#include "ctr/gameplay/body_vector_write.h"
#include "ctr/gameplay/body_record.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace ctr {
void write_body_vector(std::span<const std::byte> mapping,std::int32_t index,
 std::span<std::byte> bodies,std::array<float,4> v,std::int32_t notification_mode){
 if(index<0||static_cast<std::size_t>(index)>=mapping.size()/144)throw std::invalid_argument("Missing mapping row");
 const auto row=static_cast<std::size_t>(index)*144;
 if(!(std::to_integer<unsigned>(mapping[row+136])&1))throw std::invalid_argument("Missing lock/sleep and body write007dcb80");
 if(notification_mode==1)throw std::invalid_argument("Missing notification00b0ced0");
 const auto base=body_record_offset(mapping,index,bodies.size())-64;
 std::array<float,4> q;std::memcpy(q.data(),bodies.data()+base+16,16);
 for(unsigned i=0;i<4;++i)if(!std::isfinite(q[i])||!std::isfinite(v[i]))throw std::invalid_argument("Nonfinite body vector input");
 const float p0=q[0]*v[0],p1=q[1]*v[1],p2=q[2]*v[2];const float dot=(p0+p1)+(p2+0.f);
 const float square=q[3]*q[3];const float factor=square-0.5f;
 constexpr unsigned perm[4]={1,2,0,3};std::array<float,4>cross,out;
 for(unsigned i=0;i<4;++i){const float a=q[perm[i]]*v[i];const float b=q[i]*v[perm[i]];cross[i]=a-b;}
 for(unsigned i=0;i<4;++i){const float a=v[i]*factor,b=q[i]*dot;const float sum=a+b;const float c=q[3]*cross[perm[i]];const float sum2=sum+c;out[i]=sum2+sum2;}
 std::memcpy(bodies.data()+base+80,out.data(),16);
}
}
