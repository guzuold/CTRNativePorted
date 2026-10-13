#pragma once
#include "ctr/game_heap_api_continuation.h"
#include "ctr/heap_callback_registration.h"
namespace ctr {
class HeapInitializerBinding final {
public:
 static HeapInitializerBinding load(const std::filesystem::path&);
 std::uint64_t pointer() const noexcept {return pointer_;}
private:
 std::uint64_t pointer_{};
};
HeapCallbackRegistration continue_game_heap_callbacks(const GameHeapApiContinuation&,const HeapInitializerBinding&,std::shared_ptr<const NativeProcParam>,std::span<std::byte> slots96);
}
