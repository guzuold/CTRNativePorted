#pragma once
#include "ctr/atexit_reregistration.h"
namespace ctr {
FinalizeCallerFrame generate_reused_head_call(const NativeAtexitBss&,BoundedWalkLayout,const FinalizeCallerFrame&);
FinalizeCallerFrame complete_reused_head_return(const FinalizeCallerFrame&,const FinalizeCallerFrame&,std::span<const std::byte>,BoundedWalkLayout);
BoundedWalkResult finalize_reused_head(const std::shared_ptr<NativeAtexitBss>&,BoundedWalkLayout,const std::shared_ptr<NativeTypedGameHeapCallback>&,std::uint64_t dso_filter,std::uint64_t thread_exit_list);
std::array<std::byte,8192> serialize_reused_head_removed(const NativeAtexitBss&,BoundedWalkLayout);
}
