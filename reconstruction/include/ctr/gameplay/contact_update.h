// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "ctr/object_access.h"
namespace ctr {
struct ContactTimer {std::int32_t started,current;bool running,stopped;};
struct ContactUpdateState {bool steep;ContactTimer timer;};
// Services must execute actual reconstructed or original retail queries. No default successful implementation.
struct ContactUpdateServices {
 bool(*available)(void*);
 bool(*direction)(void*,Vec3,Vec3&);
 std::int32_t(*clock)(void*);
 bool(*notification_component)(void*);
 void(*notify)(void*,bool);
 void* context;
};
// Whole00efdeb0 source recovery; not yet validated against original.
void update_contact_state(ContactUpdateState&,const EntityLocalView&,Vec3 defaults,const ContactUpdateServices&);
}
