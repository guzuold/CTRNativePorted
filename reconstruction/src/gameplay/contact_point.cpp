#include "ctr/gameplay/contact_point.h"
#include "ctr/gameplay/mean_scale.h"
#include <bit>
#include <cmath>
#include <stdexcept>
namespace ctr {
namespace {
ContactVector perm(ContactVector v,unsigned mask){
    ContactVector r;for(unsigned i=0;i<4;++i)r[i]=v[(mask>>(2*i))&3];return r;
}
ContactVector add(ContactVector a,ContactVector b){
    ContactVector r;for(unsigned i=0;i<4;++i)r[i]=a[i]+b[i];return r;
}
ContactVector sub(ContactVector a,ContactVector b){
    ContactVector r;for(unsigned i=0;i<4;++i)r[i]=a[i]-b[i];return r;
}
ContactVector mul(ContactVector a,ContactVector b){
    ContactVector r;for(unsigned i=0;i<4;++i)r[i]=a[i]*b[i];return r;
}
ContactVector splat(float a){return {a,a,a,a};}
ContactVector basis(ContactVector p,ContactVector r,ContactVector factor,ContactVector base){
    return add(sub(mul(p,perm(factor,0x12)),mul(r,perm(factor,9))),base);
}
}
Vec3 transform_contact_point(const EntityLocalView& entity,
    const OrientationBits* orientation,const LocalScaleTransform* scale,
    OrientationBits default_orientation,Vec3 default_scale,Vec3 point,
    const ContactPointGlobals* globals) {
    if(!globals)throw std::invalid_argument("Missing initialized contact point globals");
    const auto bits=local_orientation(orientation,entity.has_parent,default_orientation);
    ContactVector q;for(unsigned i=0;i<4;++i){
        q[i]=std::bit_cast<float>(bits[i]);
        if(!std::isfinite(q[i]))throw std::invalid_argument("Non-finite quaternion domain");
    }
    const auto& g=*globals;
    const auto squared=mul(q,q);
    const float zw=squared[2]+squared[3];
    const float yzw=squared[1]+zw;
    const float norm=squared[0]+yzw;
    ContactVector x,y,z,t;
    if(norm<=2.499999990010493e-13f){
        x=g.zero_basis_x;y=g.zero_basis_y;z=g.zero_basis_z;t=g.zero_translation;
    }else{
        const auto p=perm(q,9),r=perm(q,0x12);
        const auto fx=mul(perm(q,0x1b),g.quaternion_factor_x);
        const ContactVector swapped{q[2],q[3],q[0],q[1]}; // VPERMILPD imm1
        const auto fy=mul(swapped,g.quaternion_factor_y);
        const auto fz=mul(perm(q,0xb1),g.quaternion_factor_z);
        x=basis(p,r,fx,g.basis_x);x[3]=g.homogeneous[0];
        y=basis(p,r,fy,g.basis_y);y[3]=g.homogeneous[1];
        z=basis(p,r,fz,g.basis_z);z[3]=g.homogeneous[2];
        t=g.translation;
    }
    x=mul(splat(mean_local_scale(scale,entity.has_parent,default_scale)),x);
    y=mul(splat(mean_local_scale(scale,entity.has_parent,default_scale)),y);
    z=mul(splat(mean_local_scale(scale,entity.has_parent,default_scale)),z);
    const auto position=unparented_position(entity);
    ContactVector translated{position.x,position.y,position.z,t[3]};
    const auto xy=add(mul(splat(point.x),x),mul(splat(point.y),y));
    auto result=add(xy,add(mul(splat(point.z),z),translated));
    if(result[3]!=1.0f){
        constexpr float epsilon=4.999999987376214e-7f;
        const float divisor=std::fabs(result[3])<epsilon?epsilon:result[3];
        const float reciprocal=1.0f/divisor;
        result[0]=result[0]*reciprocal;
        result[1]=result[1]*reciprocal;
        result[2]=result[2]*reciprocal;
    }
    return {result[0],result[1],result[2]};
}
}
