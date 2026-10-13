#pragma once
#include "ctr/atexit_bounded_walk.h"
#include "ctr/atexit_embedded_registration.h"
namespace ctr {
struct ReregistrationResult {AtexitCallResult call;std::uint64_t flags;std::vector<std::uint32_t> scan;};
// Numeric records only. Does not bind a callable lease or invoke a second callback.
ReregistrationResult register_removed_embedded(const std::shared_ptr<NativeAtexitBss>&,BoundedWalkLayout,const std::array<std::uint64_t,16>&,std::span<std::byte> stack128,std::uint64_t entry_flags);
std::array<std::byte,8192> serialize_reregistered(const NativeAtexitBss&,BoundedWalkLayout);
}
