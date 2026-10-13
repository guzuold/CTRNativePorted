#pragma once
#include <span>
#include <cstddef>
namespace ctr {
//00ee6d20 borrowed storage, explicit data and clock input; active manager/lifetime missing.
void reset_boost(std::span<std::byte> timer,std::span<const std::byte> source,
 std::span<const std::byte> data,int now);
}
