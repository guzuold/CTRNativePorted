#pragma once
#include <array>
#include <span>
#include <cstddef>
#include <cstdint>
namespace ctr {
//007dc990 full arithmetic after bounded mapping lookup; no Havok or normalization.
std::array<float,4> transform_body_vector(std::array<float,4> quaternion,std::array<float,4> vector);
std::array<float,4> body_vector(std::span<const std::byte> mapping,std::int32_t index,std::span<const std::byte> bodies);
}
