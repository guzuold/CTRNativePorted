// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/air_velocity_projection.h"
#include <cmath>
#include <stdexcept>
namespace ctr {
float air_velocity_projection(const EntityLocalView& entity, Vec3 defaults, Vec3 velocity) {
 const auto a=unparented_angles(entity,defaults);
 if(!std::isfinite(a.y)||!std::isfinite(a.z)) throw std::invalid_argument("Non-finite projection angles unsupported");
 const float sy=std::sin(a.y), cy=a.y==0.f?1.f:std::cos(a.y);
 const float sz=a.z==0.f?0.f:std::sin(a.z), cz=a.z==0.f?1.f:std::cos(a.z);
 const float negative_sy=a.y==0.f?-0.f:-sy;
 const float x=(cy*cz)*velocity.x;
 const float y=(cy*sz)*velocity.y;
 const float z=negative_sy*velocity.z;
 return ((x+0.f)+y)+z;
}
}
