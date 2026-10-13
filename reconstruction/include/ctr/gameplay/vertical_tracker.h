#pragma once
#include "ctr/object_access.h"
#include <cstdint>
namespace ctr {
// Adapted view of 00ef9750's state at +0/+4/+8, not the original ABI.
struct VerticalTracker { float reference_height; float accumulated; std::uint8_t active; };
void update_vertical_tracker(VerticalTracker& state, const EntityLocalView& entity,
                             float elapsed, float rate);
}
