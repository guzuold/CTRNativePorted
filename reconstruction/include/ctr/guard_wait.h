#pragma once
#include "ctr/native_guard_sync.h"
namespace ctr {
// Separate pending100 -> ready1 route; excludes lazy cell initialization and main once.
GuardCallResult invoke_pending_guard_wait(NativeGuardSync&,std::span<std::byte> guard8,const std::array<std::uint64_t,16>& entry,std::span<std::byte> stack128);
}
