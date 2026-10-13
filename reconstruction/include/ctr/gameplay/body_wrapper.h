#pragma once
#include "ctr/gameplay/body_vector_write.h"
namespace ctr {
//00c3c5b0 scales3inputs by original degrees-to-radians constant; fourth lane zero.
void write_body_angles(std::span<const std::byte> mapping,std::int32_t index,std::span<std::byte> bodies,
 std::array<float,3> angles,std::int32_t notification_mode);
}
