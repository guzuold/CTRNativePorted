#pragma once
#include "ctr/native_procparam.h"
#include <array>
namespace ctr {
struct HeapCallbackRegistration {
 std::shared_ptr<const NativeProcParam> parameters;
 // RAX,RBX,RCX,RDX,RSI,RDI,R8..R15,RSP,RBP at actual helper return.
 std::array<std::uint64_t,16> return_registers{};
};
// Full a96d0 positive genuine heapparam path. Callback IDs are not callable.
HeapCallbackRegistration register_game_heap_callbacks(std::shared_ptr<const NativeProcParam>,std::span<std::byte> slots96,const std::array<std::uint64_t,16>& entry_registers);
}
