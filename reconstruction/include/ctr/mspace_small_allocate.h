#pragma once
#include "ctr/mspace_allocate.h"
namespace ctr {
// Exact-fit small-bin head only, valid borrowed heap with no dv/tree/threshold/tracking/time.
MspaceAllocation allocate_mspace_small_exact(std::span<std::byte> backing,
    const MspaceTopAllocationContext& context,std::uint64_t request);
// Verified exact64 bin head priority while valid64-byte dv remains present.
MspaceAllocation allocate_mspace_exact_with_dv(std::span<std::byte> backing,
    const MspaceTopAllocationContext& context,std::uint64_t request);
}
