// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/entity_distance_squared.h"
#include <bit>
#include <cstdint>
namespace ctr {
float entity_distance_squared(const EntityLocalView& first,const EntityLocalView* second){
 if(!second)return std::bit_cast<float>(std::uint32_t{0x7f7fffff});
 const auto a=unparented_position(first),b=unparented_position(*second);
 const float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;
 return ((x*x+0.f)+y*y)+z*z;
}
}
