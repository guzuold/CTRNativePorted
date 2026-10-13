#pragma once
#include "ctr/mspace_allocate.h"
namespace ctr {
// Valid PREPARED dv, no bins/tree/threshold/tracking/time; no dv creation claimed.
MspaceAllocation allocate_mspace_dv(std::span<std::byte> backing,
    const MspaceTopAllocationContext& context,std::uint64_t request);
// Allocated chunk adjacent to prepared dv, with allocated other neighbor or top.
void free_mspace_dv_adjacent(std::span<std::byte> backing,
    const MspaceTopAllocationContext& context,std::uint64_t payload_token);
}
