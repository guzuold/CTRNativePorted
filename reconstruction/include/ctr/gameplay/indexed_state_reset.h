// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
namespace ctr {
// Decoded retail state fields +0c/+10/+14; descriptive names chosen here.
struct IndexedState { std::uint32_t current,previous;std::uint16_t flags; };
struct IndexedStateResetServices {
 std::uint32_t (*query_index)(void*); // controller+e8 virtual+b0, whole genuine method
 void* (*find_component)(void*); // whole004d9a90(owner+18, registered type,0)
 void (*activate_component)(void*,std::uint32_t,void*); // whole00ef94b0(component,1)
 void* context;
};
// Whole00eea4c0; compiled reconstruction, not original-verified or installed.
void indexed_state_reset(IndexedState&,const IndexedStateResetServices&);
}
