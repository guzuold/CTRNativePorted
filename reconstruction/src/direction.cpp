#include "ctr/direction.h"
#include <cmath>
#include <stdexcept>

namespace ctr {
namespace {
Vec3 angles(const EntityLocalView& entity, Vec3 defaults) {
    const auto a = unparented_angles(entity, defaults);
    if (!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(a.z))
        throw std::invalid_argument("Non-finite direction angles are unsupported");
    return a;
}
float sine(float a) { return a == 0.f ? 0.f : std::sin(a); }
float cosine(float a) { return a == 0.f ? 1.f : std::cos(a); }
}
Vec3 entity_up(const EntityLocalView& entity, Vec3 defaults) {
    const auto a = angles(entity, defaults);
    const float sx = sine(a.x), cx = cosine(a.x);
    const float sy = sine(a.y), cy = cosine(a.y);
    const float sz = sine(a.z), cz = cosine(a.z);
    // The original skips this product for zero z; preserve its positive zero.
    const float szcx = a.z == 0.f ? 0.f : sz * cx;
    return {sx * sz + sy * (cx * cz), sy * szcx - sx * cz, cx * cy};
}
Vec3 entity_side(const EntityLocalView& entity, Vec3 defaults) {
    const auto a = angles(entity, defaults);
    const float sx = sine(a.x), cx = cosine(a.x);
    const float sy = sine(a.y), cy = cosine(a.y);
    const float sz = sine(a.z), cz = cosine(a.z);
    const float sysx = a.y == 0.f ? 0.f : sy * sx;
    const float cxsz = a.z == 0.f ? 0.f : cx * sz;
    return {sysx * cz - cxsz, sysx * sz + cx * cz, cy * sx};
}
}
