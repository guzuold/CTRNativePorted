#pragma once
#include <cstdint>
namespace ctr {
struct LargerSmallBin {std::uint32_t index,size,sentinel_offset,head_slot_offset;};
// Bounded original selection only; caller supplies bitmap at the larger-bin branch.
// This does not allocate, split, create dv, or assert the supplied heap is valid.
LargerSmallBin select_larger_small_bin(std::uint32_t bitmap,std::uint32_t required_size);
}
