#pragma once
#include "ctr/native_mutex.h"
namespace ctr {
struct MspaceTopAllocationContext {
    std::uint64_t base_token{},header_token{};
    bool poison{};
    NativeMutex* mutex{};
};
struct MspaceAllocation {std::uint64_t address_token{};std::span<std::byte> payload;};
// Actual malloc return under empty-bin/dv/tree, no tracking/time and sufficient top contracts.
MspaceAllocation allocate_mspace_top(std::span<std::byte> backing,
    const MspaceTopAllocationContext& context,std::uint64_t request);
}
