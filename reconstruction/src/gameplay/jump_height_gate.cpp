// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/gameplay/jump_height_gate.h"
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>
namespace ctr {
bool jump_height_gate(const EntityLocalView& entity, Vec3 defaults, bool active, float age, float height) {
 const auto angles=unparented_angles(entity,defaults);
 if(!std::isfinite(angles.y)||!std::isfinite(age)||!std::isfinite(height)) throw std::invalid_argument("Non-finite jump gate inputs unsupported");
 const float sy=angles.y==0.f?0.f:std::sin(angles.y);
 if(!(-0.5f>sy)||active||age<std::bit_cast<float>(std::uint32_t{0x3f7ff605})||height==0.f) return false;
 const auto p=unparented_position(entity);
 if(!std::isfinite(p.z)) throw std::invalid_argument("Non-finite jump position unsupported");
 return 150.f>(p.z-height);
}
}
