#pragma once
#include "ctr/object_access.h"
namespace ctr {
// Whole004d9ff0 borrowed local view; original transform fields +6c/+70/+74/+78.
struct LocalScaleTransform { float uniform; Vec3 axes; };
Vec3 local_scale(const LocalScaleTransform* transform, bool has_parent,
                Vec3 original_default);
}
