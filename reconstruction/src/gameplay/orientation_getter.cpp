#include "ctr/gameplay/orientation_getter.h"
#include <stdexcept>
namespace ctr {
OrientationBits local_orientation(const OrientationBits* transform, bool has_parent,
                                  OrientationBits original_default) {
    if (has_parent) throw std::invalid_argument("Missing parent quaternion virtual+c0");
    return transform ? *transform : original_default;
}
}
