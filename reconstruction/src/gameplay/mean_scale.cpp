#include "ctr/gameplay/mean_scale.h"
namespace ctr {
float mean_local_scale(const LocalScaleTransform* transform, bool has_parent,
                       Vec3 original_default) {
    const auto v = local_scale(transform,has_parent,original_default);
    const float xy = v.x + v.y;
    const float xyz = xy + v.z;
    return xyz / 3.0f; // Original constant at flat1b26fec.
}
}
