// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "ctr/gameplay/contact_point.h"
#include <span>
namespace ctr {
// Whole00efe5d0 borrowed entity/geometry views; original output is in/out.
float project_contact_point(const EntityLocalView* entity,const std::byte* geometry,
 const OrientationBits* orientation,const LocalScaleTransform* scale,
 OrientationBits default_orientation,Vec3 default_scale,Vec3 input,Vec3& output,
 const std::array<float,7>* matrix_globals,const ContactPointGlobals* point_globals,
 const std::uint64_t& guard);
}
