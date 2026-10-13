#pragma once
#include "ctr/gameplay/scale_getter.h"
namespace ctr {
float mean_local_scale(const LocalScaleTransform* transform, bool has_parent,
                       Vec3 original_default);
}
