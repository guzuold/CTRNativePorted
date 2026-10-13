// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/contact_projection.h"
#include "ctr/gameplay/inverse_point.h"
#include "ctr/core/contact_curve_collection.h"
#include "ctr/core/contact_sample.h"
namespace ctr {
float project_contact_point(const EntityLocalView* entity,const std::byte* geometry,
 const OrientationBits* orientation,const LocalScaleTransform* scale,
 OrientationBits default_orientation,Vec3 default_scale,Vec3 input,Vec3& output,
 const std::array<float,7>* matrix_globals,const ContactPointGlobals* point_globals,
 const std::uint64_t& guard){
 float position=0.0f;
 if(!entity||!geometry)return position;
 const auto local=inverse_point(*entity,scale,default_scale,input,matrix_globals);
 const std::array<float,3> query{local.x,local.y,local.z};
 float squared_distance=0.0f;
 // Original dfe645 ignores collection EAX, retaining its explicit outputs.
 contact_curve_collection(std::span<const std::byte,80>(geometry,80),query,
                          position,squared_distance,2,guard);
 // Original dfe65a ignores sampler EAX. Preserve the preexisting outpoint
 // if the sampler's verified no-write branch is taken.
 std::array<float,3> sampled{output.x,output.y,output.z};
 contact_sample(std::span<const std::byte,112>(geometry,112),position,sampled);
 output=transform_contact_point(*entity,orientation,scale,default_orientation,
         default_scale,{sampled[0],sampled[1],sampled[2]},point_globals);
 return position;
}
}
