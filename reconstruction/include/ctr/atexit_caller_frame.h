#pragma once
#include "ctr/native_atexit_bss.h"
#include <array>
namespace ctr {
struct FinalizeCallerFrame {
 std::array<std::byte,1024> stack;
 std::array<std::uint64_t,16> registers;
 std::uint64_t flags{};
};
// Logical original stack 2000fc00..20010000. Actual owned source registry head
// is checked before translating its identity. NULL DSO/one first kind-2 only.
// Cookie slot receives this compiled module's actual security cookie.
FinalizeCallerFrame generate_single_finalize_call(const NativeAtexitBss& registry,
 const FinalizeCallerFrame& entry);
// Compose the VERIFIED callback output with the bounded helper/epilogue
// logical stack effects after real services have returned. No physical ABI.
FinalizeCallerFrame complete_single_finalize_return(const FinalizeCallerFrame& entry,
 const FinalizeCallerFrame& generated,std::span<const std::byte> callback_output);
FinalizeCallerFrame generate_middle_node_finalize_call(const NativeAtexitBss& registry,
 const FinalizeCallerFrame& entry);
FinalizeCallerFrame complete_middle_node_finalize_return(const FinalizeCallerFrame& entry,
 const FinalizeCallerFrame& generated,std::span<const std::byte> callback_output);
FinalizeCallerFrame generate_disabled_head_finalize_call(const NativeAtexitBss& registry,
 const FinalizeCallerFrame& entry);
FinalizeCallerFrame complete_disabled_head_finalize_return(const FinalizeCallerFrame& entry,
 const FinalizeCallerFrame& generated,std::span<const std::byte> callback_output);
FinalizeCallerFrame generate_two_node_finalize_call(const NativeAtexitBss& registry,
 const FinalizeCallerFrame& entry);
FinalizeCallerFrame complete_two_node_finalize_return(const FinalizeCallerFrame& entry,
 const FinalizeCallerFrame& generated,std::span<const std::byte> callback_output);
}
