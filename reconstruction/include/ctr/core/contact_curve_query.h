#pragma once
#include <cstdint>
#include <span>
namespace ctr {
float contact_curve_query(std::span<const float,32> control,std::span<const float,3> start,std::span<const float,3> end,std::span<const float,3> query,float first,float last,std::int32_t axis,float& projected,const std::uint64_t& guard);
}
