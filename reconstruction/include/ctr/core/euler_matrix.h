#pragma once
#include <span>
namespace ctr {
void euler_matrix(std::span<float,16> output, float x,float y,float z,
                  std::span<const float,7> homogeneous_globals);
}
