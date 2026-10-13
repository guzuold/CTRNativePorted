#include "ctr/gameplay/body_wrapper.h"
namespace ctr {
void write_body_angles(std::span<const std::byte> mapping,std::int32_t index,std::span<std::byte> bodies,
 std::array<float,3> angles,std::int32_t notification_mode){
 constexpr float scale=0.01745329238474369f;
 std::array<float,4> requested{angles[0]*scale,angles[1]*scale,angles[2]*scale,0.f};
 write_body_vector(mapping,index,bodies,requested,notification_mode);
}
}
