#pragma once
#include "ctr/atexit_single_finalize.h"
#include "ctr/atexit_caller_frame.h"
namespace ctr {
struct BoundedWalkLayout {std::uint32_t count{},live{};};
struct BoundedWalkResult {SingleFinalizeResult finalizer;std::vector<std::uint32_t> order;};
// Exclusive source-owned contiguous initial table, 1..64 genuine records,
// exactly one live kind2 and prepared kind0 identities elsewhere. No generic
// node ownership, concurrent mutation, non-null DSO or thread-exit callbacks.
BoundedWalkLayout validate_bounded_walk(const NativeAtexitBss& registry);
std::array<std::byte,8192> serialize_bounded_walk(const NativeAtexitBss& registry,BoundedWalkLayout layout,bool removed);
FinalizeCallerFrame generate_bounded_walk_call(const NativeAtexitBss& registry,const FinalizeCallerFrame& entry);
FinalizeCallerFrame complete_bounded_walk_return(const FinalizeCallerFrame& entry,const FinalizeCallerFrame& generated,
 std::span<const std::byte> callback_output,BoundedWalkLayout layout);
BoundedWalkResult finalize_bounded_walk(const std::shared_ptr<NativeAtexitBss>& registry,
 const std::shared_ptr<NativeTypedGameHeapCallback>& callback,std::uint64_t dso_filter,std::uint64_t thread_exit_list);
}
