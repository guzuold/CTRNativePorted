#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Whole014d3a10. Borrowed tables/cache; synthetic immutable guard contract.
float contact_curve_search(std::span<const std::byte,128> receiver,std::span<const float,3> query,float& squared_distance,std::int32_t axis,const std::uint64_t& guard);
}
