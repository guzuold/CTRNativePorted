#pragma once
#include "ctr/scalar_speed.h"
#include <span>
#include <cstddef>
namespace ctr {
struct ScalarQueryView {std::span<const std::byte> image;std::uint64_t base,object,expected,igobject_type;};
//00eff240 whole interface: unsupported handle/component/scale paths explicitly missing.
SpeedResult advance_registered_scalar(float speed,float dt,float steering,
 const LinearVelocityProfile& profile,SpeedMode mode,const ScalarQueryView& query,
 float factor178,float factor180,float special184,bool handle98,bool handleb8);
}
