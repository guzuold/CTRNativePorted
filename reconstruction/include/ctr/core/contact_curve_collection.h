#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Whole014d4670; receiver+18 signed count,+40 borrowed curve array (stride88hex).
std::uint32_t contact_curve_collection(std::span<const std::byte,80> receiver,std::span<const float,3> query,float& position,float& squared_distance,std::int32_t axis,const std::uint64_t& guard);
}
