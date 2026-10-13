#pragma once
#include "ctr/mspace_allocate.h"
namespace ctr {
// Real larger-small-bin split creating dv; old dv absent, no tree/threshold/tracking/time.
MspaceAllocation allocate_mspace_larger_split(std::span<std::byte> backing,
    const MspaceTopAllocationContext& context,std::uint64_t request);
// Original split replaces insufficient small dv by inserting it into its small bin.
MspaceAllocation allocate_mspace_replace_dv(std::span<std::byte> backing,
    const MspaceTopAllocationContext& context,std::uint64_t request);
}
