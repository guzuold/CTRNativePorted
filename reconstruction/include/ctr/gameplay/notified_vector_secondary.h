// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "ctr/object_access.h"
namespace ctr {
// Whole00c7c4a0; adapted vector view for original object+0x6c.
// Finite no-change domain verified separately; changed path requires genuine virtual+b8.
bool update_secondary_notified_vector(Vec3& current, Vec3 requested, VectorNotification notify, void* context);
}
