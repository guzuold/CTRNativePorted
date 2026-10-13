#pragma once
#include "ctr/mspace_allocate.h"
namespace ctr {
// Borrowed non-global heap. Small-bin backward/forward unlink and top merge;
// coalesced free chunks inserted in bins must remain below 256 bytes.
void free_mspace_coalescing(std::span<std::byte> backing,
    const MspaceTopAllocationContext& context,std::uint64_t payload_token);
}
