// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "ctr/gameplay/entity_matrix.h"
namespace ctr {
// Whole original00cd4240; borrowed globals and parent domain match entity_matrix.
Vec3 inverse_point(const EntityLocalView& entity,const LocalScaleTransform* scale,
 Vec3 default_scale,Vec3 point,const std::array<float,7>* initialized_globals);
}
