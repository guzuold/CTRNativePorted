#pragma once
#include "ctr/native_game_dso.h"
#include "ctr/game_heap_destructor.h"
#include <atomic>
#include <memory>
#include <vector>

namespace ctr {
inline constexpr std::uint64_t original_game_heap_callback_id=0x01437910;
inline constexpr std::uint64_t original_game_heap_argument_id=0x026ee0b8;
inline constexpr std::uint64_t original_game_dso_id=0x02600090;

struct TypedGameHeapCallbackStatus {
 std::uintptr_t dso_address{};
 std::uintptr_t code_module{};
 std::uint64_t callback_owner_references{};
 bool invoked{};
 bool backend_released{};
};

// Explicit typed adapter for three validated binary identities. IDs remain data;
// only the compiled Callback type below is invoked.
class NativeTypedGameHeapCallback final {
 using Callback=GameHeapDestructorResult (*)(NativeTypedGameHeapCallback&);
 struct CodeModuleLease;
 std::shared_ptr<NativeGameDso> dso_;
 std::shared_ptr<CodeModuleLease> code_;
 std::shared_ptr<SystemPoolBackend> backend_;
 std::array<std::byte,431> input_{};
 std::array<std::byte,32> manager_{};
 Callback callback_{};
 std::atomic_flag invoking_=ATOMIC_FLAG_INIT;
 bool invoked_{};
 bool finalizer_frame_{};

 static GameHeapDestructorResult invoke_compiled_callback(NativeTypedGameHeapCallback&);
 GameHeapDestructorResult execute_original_contract();
public:
 NativeTypedGameHeapCallback(const std::filesystem::path&,std::span<const std::byte> input);
 NativeTypedGameHeapCallback(const NativeTypedGameHeapCallback&)=delete;
 NativeTypedGameHeapCallback& operator=(const NativeTypedGameHeapCallback&)=delete;
 std::vector<std::byte> dispatch(std::uint64_t callback_id,std::uint64_t argument_id,
                                 std::uint64_t dso_id);
 TypedGameHeapCallbackStatus status() const;
 // Measured original CALL-r13 frame; identities are still validated as data.
 std::vector<std::byte> dispatch_finalizer(std::uint64_t callback_id,
     std::uint64_t argument_id,std::uint64_t dso_id);
 void validate_finalizer_frame() const;
};
}
