#pragma once
#include "ctr/system_pool_backend.h"
#include "ctr/mspace_allocate.h"
namespace ctr {
// Explicit prepared installation, original identities are not native generic function pointers.
struct PreparedGameGlobalMalloc {SystemPoolBackend* backend{};std::uint64_t original_malloc_target{0x01437990};bool poison{};};
// Full libc58a30 -> game01537990 -> original mspace malloc under successful top-only contract.
MspaceAllocation allocate_game_global_top(const PreparedGameGlobalMalloc& binding,std::uint64_t request);
}
