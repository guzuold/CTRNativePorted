#pragma once
#include "ctr/gameplay/orientation_getter.h"
#include "ctr/gameplay/scale_getter.h"
#include <array>
namespace ctr {
using ContactVector = std::array<float,4>;
// Borrowed values of mutable original globals, not an initializer implementation.
struct ContactPointGlobals {
    ContactVector basis_x, basis_y, basis_z, translation;
    ContactVector quaternion_factor_x, quaternion_factor_y, quaternion_factor_z;
    ContactVector homogeneous;
    ContactVector zero_basis_x, zero_basis_y, zero_basis_z, zero_translation;
};
// Whole00cd4370 adapted view; unresolved parent callbacks throw via getters.
Vec3 transform_contact_point(const EntityLocalView& entity,
    const OrientationBits* orientation, const LocalScaleTransform* scale,
    OrientationBits default_orientation, Vec3 default_scale, Vec3 point,
    const ContactPointGlobals* globals);
}
