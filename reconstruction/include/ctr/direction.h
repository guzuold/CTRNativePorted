#pragma once
#include "ctr/object_access.h"

namespace ctr {
// 00cd2180 and 00cd22f0, with explicit host float sin/cos services.
// Only the unparented angle getter is reconstructed.
Vec3 entity_up(const EntityLocalView& entity, Vec3 default_angles);
Vec3 entity_side(const EntityLocalView& entity, Vec3 default_angles);
}
