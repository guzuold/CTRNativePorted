#include "ctr/core/euler_matrix.h"
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>
namespace ctr {
void euler_matrix(std::span<float,16> m,float x,float y,float z,std::span<const float,7> g){
 if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))throw std::invalid_argument("Nonfinite trig domain missing");
 float sx=std::sin(x);float cx=std::cos(x);
 if(x==0.0f){sx=0.0f;cx=1.0f;}
 float sy=0.0f,cy=1.0f,xy=0.0f;
 if(y!=0.0f){sy=std::sin(y);cy=std::cos(y);xy=sy*sx;}
 float sz=0.0f,cz=1.0f,cs=0.0f;
 if(z!=0.0f){sz=std::sin(z);cz=std::cos(z);cs=cx*sz;}
 float cc=cx*cz;
 m[0]=cy*cz;m[1]=cy*sz;
 m[2]=std::bit_cast<float>(std::bit_cast<std::uint32_t>(sy)^0x80000000u);
 float t=xy*cz;t=t-cs;m[4]=t;
 float u=sy*cs;
 t=xy*sz;t=t+cc;m[5]=t;
 float v=sy*cc;
 m[6]=sx*cy;
 t=sx*sz;t=t+v;m[8]=t;
 t=sx*cz;u=u-t;m[9]=u;
 m[10]=cx*cy;
 for(unsigned i=0;i<4;++i)m[12+i]=g[i];
 m[3]=g[4];m[7]=g[5];m[11]=g[6];
}
}
