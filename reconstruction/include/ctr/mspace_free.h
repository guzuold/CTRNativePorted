#pragma once
#include "ctr/mspace_allocate.h"
namespace ctr {
// Borrowed backing, non-global heap; only the last allocated chunk merging into top.
// Unsupported paths throw before heap mutation. Freed spans cease to be valid.
void free_mspace_top(std::span<std::byte> backing,
    const MspaceTopAllocationContext& context,std::uint64_t payload_token);
}
