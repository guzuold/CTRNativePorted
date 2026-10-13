// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "ctr/gameplay/contact_projection.h"
namespace ctr {
// Whole00efe220. Explicit borrowed preexisting stack values bound by fixtures;
// original no-write sampler branches otherwise have unresolved provenance.
struct ContactDirectionScratch { Vec3 projected; Vec3 sampled; };
Vec3 contact_direction(const EntityLocalView* entity,const std::byte* geometry,
 const OrientationBits* orientation,const LocalScaleTransform* scale,
 OrientationBits default_orientation,Vec3 default_scale,Vec3 input,Vec3* projected_output,
 ContactDirectionScratch scratch,const std::array<float,7>* matrix_globals,
 const ContactPointGlobals* point_globals,const std::uint64_t& guard);
}
