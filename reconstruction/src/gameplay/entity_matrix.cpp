#include "ctr/gameplay/entity_matrix.h"
#include "ctr/core/euler_matrix.h"
#include "ctr/core/matrix_adjugate.h"
#include "ctr/gameplay/mean_scale.h"
#include <cmath>
#include <stdexcept>
namespace ctr { namespace {
using V=std::array<float,4>;
V mul(V a,V b){for(unsigned i=0;i<4;++i)a[i]=a[i]*b[i];return a;}
V add(V a,V b){for(unsigned i=0;i<4;++i)a[i]=a[i]+b[i];return a;}
V sub(V a,V b){for(unsigned i=0;i<4;++i)a[i]=a[i]-b[i];return a;}
V scalar(V a,V b,char op){a[0]=op=='*'?a[0]*b[0]:op=='+'?a[0]+b[0]:a[0]-b[0];return a;}
V perm(V a,unsigned k){V r;for(unsigned i=0;i<4;++i)r[i]=a[(k>>(2*i))&3];return r;}
V insert(V a,V b,unsigned k){a[(k>>4)&3]=b[(k>>6)&3];for(unsigned i=0;i<4;++i)if(k&(1u<<i))a[i]=0;return a;}
V blend(V a,V b,unsigned k){for(unsigned i=0;i<4;++i)if(k&(1u<<i))a[i]=b[i];return a;}
float determinant(const std::array<float,16>& m){
 V v0{};
 V v1{};
 V v2{};
 V v3{};
 V v4{};
 V v5{};
 V v6{};
 V v7{};
 V v8{};
 V v9{};
 V v10{};
 V v11{};
 V v12{};
 V v13{};
 V v14{};
 v13={m[12],0,0,0};v0={m[13],0,0,0};v5={m[13],m[13],m[13],m[13]};
 v1=V{m[14],m[15],0,0}; // 0xbc8366
 v6=V{m[10],m[11],0,0}; // 0xbc836b
 v4=V{m[8],0,0,0}; // 0xbc8370
 v9=V{m[5],0,0,0}; // 0xbc8375
 v12=V{m[9],0,0,0}; // 0xbc837a
 v10=V{m[4],0,0,0}; // 0xbc837f
 v8=V{m[6],0,0,0}; // 0xbc8384
 v3=insert(v4,v6,28); // 0xbc8389
 v2=insert(v13,v1,28); // 0xbc838f
 v5=mul(v6,v5); // 0xbc8395
 v4=perm(v4,0); // 0xbc8399
 v0=blend(v1,v0,1); // 0xbc839f
 v3=mul(v3,v1); // 0xbc83a5
 v2=mul(v6,v2); // 0xbc83a9
 v0=mul(v4,v0); // 0xbc83ad
 v4=perm(v13,0); // 0xbc83b1
 v14=sub(v3,v2); // 0xbc83b7
 v2=perm(v14,245); // 0xbc83bb
 v11=scalar(v9,v2,'*'); // 0xbc83c0
 v2=perm(v12,0); // 0xbc83c4
 v2=mul(v2,v1); // 0xbc83ca
 v1=blend(v6,v12,1); // 0xbc83ce
 v1=mul(v1,v4); // 0xbc83d4
 v2=sub(v2,v5); // 0xbc83d8
 v5=V{m[7],0,0,0}; // 0xbc83dc
 v0=sub(v0,v1); // 0xbc83e1
 v1=perm(v0,245); // 0xbc83e5
 v4=insert(v5,v10,16); // 0xbc83e9
 v1=scalar(v8,v1,'*'); // 0xbc83ef
 v7=insert(v5,v8,16); // 0xbc83f3
 v5=insert(v5,v9,16); // 0xbc83f9
 v4=mul(v4,v14); // 0xbc83ff
 v5=mul(v5,v0); // 0xbc8404
 v7=mul(v7,v2); // 0xbc8408
 v6=perm(v4,245); // 0xbc840c
 v3=perm(v7,245); // 0xbc8410
 v1=scalar(v6,v1,'-'); // 0xbc8414
 v6=perm(v5,245); // 0xbc8418
 v3=scalar(v11,v3,'-'); // 0xbc841c
 v1=scalar(v4,v1,'+'); // 0xbc8420
 v4=perm(v2,245); // 0xbc8424
 v3=scalar(v7,v3,'+'); // 0xbc8428
 v4=scalar(v10,v4,'*'); // 0xbc842c
 v4=scalar(v4,v6,'-'); // 0xbc8430
 v6=V{m[1],0,0,0}; // 0xbc8434
 v2=insert(v3,v2,28); // 0xbc8439
 v1=insert(v1,v14,28); // 0xbc843f
 v4=scalar(v5,v4,'+'); // 0xbc8445
 v5=V{m[0],0,0,0}; // 0xbc8449
 v3=insert(v6,v9,16); // 0xbc844d
 v1=mul(v3,v1); // 0xbc8453
 v0=insert(v4,v0,28); // 0xbc8457
 v5=insert(v5,v10,16); // 0xbc845d
 v2=mul(v5,v2); // 0xbc8463
 v1=sub(v2,v1); // 0xbc8467
 v2=V{m[2],0,0,0}; // 0xbc846b
 v2=insert(v2,v8,16); // 0xbc8470
 v0=mul(v2,v0); // 0xbc8476
 v0=add(v0,v1); // 0xbc847a
 v1=perm(v0,245); // 0xbc847e
 v1=scalar(v1,V{m[3],0,0,0},'*'); // 0xbc8482
 v0=scalar(v0,v1,'-'); // 0xbc8487
 return v0[0];
}
}
void entity_matrix(std::array<float,16>& m,const EntityLocalView& entity,
 const LocalScaleTransform* scale,Vec3 default_scale,
 unsigned mode,const std::array<float,7>* globals){
 if(!globals)throw std::invalid_argument("Missing initialized matrix globals");
 const auto angles=unparented_angles(entity,{(*globals)[4],(*globals)[5],(*globals)[6]});
 euler_matrix(m,angles.x,angles.y,angles.z,*globals);
 if(mode){
  // Original bc8704..bc8737 stores the rotation and borrowed homogeneous
  // fields transposed relative to the scaled mode and core Euler layout.
  for(unsigned row=0;row<4;++row)
   for(unsigned column=row+1;column<4;++column){
    const float value=m[row*4+column];
    m[row*4+column]=m[column*4+row];m[column*4+row]=value;
   }
  return;
 }
 const float sx=mean_local_scale(scale,entity.has_parent,default_scale);
 const float sy=mean_local_scale(scale,entity.has_parent,default_scale);
 const float sz=mean_local_scale(scale,entity.has_parent,default_scale);
 for(unsigned i=0;i<4;++i){m[i]=m[i]*sx;m[4+i]=m[4+i]*sy;m[8+i]=m[8+i]*sz;}
 const auto p=unparented_position(entity);m[12]=p.x;m[13]=p.y;m[14]=p.z;
 const float det=determinant(m);
 if(std::fabs(det)<4.999999987376214e-7f)return;
 matrix_adjugate(m,m);
 const float reciprocal=1.0f/det;
 for(auto& value:m)value=value*reciprocal;
}
}
