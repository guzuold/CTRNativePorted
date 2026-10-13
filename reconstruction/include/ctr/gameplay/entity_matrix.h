#pragma once
#include "ctr/gameplay/scale_getter.h"
#include <array>
namespace ctr {
// Whole00cc8120; adapted view. Core adjugate remains unverified dependency.
void entity_matrix(std::array<float,16>& output,const EntityLocalView& entity,
 const LocalScaleTransform* scale,Vec3 default_scale,
 unsigned mode,const std::array<float,7>* initialized_globals);
}
