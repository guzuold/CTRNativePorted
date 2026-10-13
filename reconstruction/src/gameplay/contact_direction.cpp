// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/contact_direction.h"
#include "ctr/core/contact_sample.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace ctr {
Vec3 contact_direction(const EntityLocalView* entity,const std::byte* geometry,
 const OrientationBits* orientation,const LocalScaleTransform* scale,
 OrientationBits default_orientation,Vec3 default_scale,Vec3 input,Vec3* projected_output,
 ContactDirectionScratch scratch,const std::array<float,7>* matrix_globals,
 const ContactPointGlobals* point_globals,const std::uint64_t& guard){
 float position=project_contact_point(entity,geometry,orientation,scale,
   default_orientation,default_scale,input,scratch.projected,matrix_globals,point_globals,guard);
 if(projected_output)*projected_output=scratch.projected;
 if(!geometry)throw std::invalid_argument("Missing contact geometry");
 float range;std::memcpy(&range,geometry+0x68,4);
 const float threshold=range+-1.0f; // original constant1b9aedc
 if(position>=threshold)position=position+-1.0f;
 position=position+1.0f; // original constant1b9aee0
 std::array<float,3> sampled{scratch.sampled.x,scratch.sampled.y,scratch.sampled.z};
 contact_sample(std::span<const std::byte,112>(geometry,112),position,sampled);
 Vec3 next{sampled[0],sampled[1],sampled[2]};
 if(entity)next=transform_contact_point(*entity,orientation,scale,default_orientation,
                        default_scale,next,point_globals);
 const float x=next.x-scratch.projected.x;
 const float y=next.y-scratch.projected.y;
 const float z=next.z-scratch.projected.z;
 const float xx=x*x;float squared=xx+0.0f;
 const float yy=y*y;squared=squared+yy;
 const float zz=z*z;squared=squared+zz;
 squared=squared+1.1754943508222875e-38f; // original1b9aee4
 const float reciprocal=1.0f/std::sqrt(squared);
 return {reciprocal*x,reciprocal*y,reciprocal*z};
}
}
