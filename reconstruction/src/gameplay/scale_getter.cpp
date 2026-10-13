#include "ctr/gameplay/scale_getter.h"
#include <cmath>
#include <stdexcept>
namespace ctr {
Vec3 local_scale(const LocalScaleTransform* transform, bool has_parent,
                Vec3 original_default) {
    if (has_parent) throw std::invalid_argument("Missing parent scale virtual+c8");
    if (!transform) return original_default;
    if (!std::isfinite(transform->uniform) || !std::isfinite(transform->axes.x) ||
        !std::isfinite(transform->axes.y) || !std::isfinite(transform->axes.z))
        throw std::invalid_argument("Non-finite scale multiplication domain");
    return {transform->uniform * transform->axes.x,
            transform->uniform * transform->axes.y,
            transform->uniform * transform->axes.z};
}
}
