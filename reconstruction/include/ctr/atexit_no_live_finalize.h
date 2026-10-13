#pragma once
#include "ctr/atexit_reused_head_finalize.h"
namespace ctr {
struct NoLiveFinalizeResult {FinalizeCallerFrame frame;FinalizeCallerFrame helper_entry;std::array<std::byte,8192> registry;std::vector<std::uint32_t> order;std::uint64_t actual_thread;};
// Exact no-live layout after two verified cycles. No callback/backend argument.
NoLiveFinalizeResult finalize_no_live(const std::shared_ptr<NativeAtexitBss>&,BoundedWalkLayout,const FinalizeCallerFrame&,std::uint64_t dso_filter,std::uint64_t thread_exit_list);
}
