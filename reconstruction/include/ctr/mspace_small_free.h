#pragma once
#include "ctr/mspace_allocate.h"
namespace ctr {
// Borrowed non-global heap: small-bin insertion and backward small-bin unlink,
// followed by top merge or small-bin insertion. Forward free/tree/dv excluded.
void free_mspace_small(std::span<std::byte> backing,
    const MspaceTopAllocationContext& context,std::uint64_t payload_token);
}
