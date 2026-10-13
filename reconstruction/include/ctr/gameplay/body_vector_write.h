#pragma once
#include <array>
#include <span>
#include <cstddef>
#include <cstdint>
namespace ctr {
//00b72e10 whole borrowed prepared-world view. Lock/sleep/notification routes missing.
void write_body_vector(std::span<const std::byte> mapping,std::int32_t index,
 std::span<std::byte> bodies,std::array<float,4> requested,std::int32_t notification_mode);
}
