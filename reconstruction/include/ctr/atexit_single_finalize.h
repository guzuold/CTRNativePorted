#pragma once
#include "ctr/native_atexit_bss.h"
#include "ctr/typed_game_heap_callback.h"
namespace ctr {
struct SingleFinalizeResult {
 std::vector<std::byte> callback_snapshots;
 std::array<std::byte,8192> callback_registry;
 std::uint64_t actual_thread{};
 std::uint32_t selected_callbacks{};
};
// Exclusive single embedded kind-2 node, NULL DSO, threadflag=0, empty
// thread-exit list. Callback stack/registers are measured input, not a native
// Windows frame or a reconstructed original caller-frame generator.
SingleFinalizeResult finalize_single_embedded(
 const std::shared_ptr<NativeAtexitBss>& registry,
 const std::shared_ptr<NativeTypedGameHeapCallback>& callback,
 std::uint64_t dso_filter,std::uint64_t thread_exit_list);
SingleFinalizeResult finalize_two_node_disabled_predecessor(
 const std::shared_ptr<NativeAtexitBss>& registry,
 const std::shared_ptr<NativeTypedGameHeapCallback>& callback,
 std::uint64_t dso_filter,std::uint64_t thread_exit_list);
std::array<std::byte,8192> serialize_two_node_registry(const NativeAtexitBss& registry,bool live_removed);
SingleFinalizeResult finalize_two_node_disabled_head(
 const std::shared_ptr<NativeAtexitBss>& registry,
 const std::shared_ptr<NativeTypedGameHeapCallback>& callback,
 std::uint64_t dso_filter,std::uint64_t thread_exit_list);
std::array<std::byte,8192> serialize_disabled_head_registry(const NativeAtexitBss& registry,bool live_removed);
SingleFinalizeResult finalize_three_node_live_middle(
 const std::shared_ptr<NativeAtexitBss>& registry,
 const std::shared_ptr<NativeTypedGameHeapCallback>& callback,
 std::uint64_t dso_filter,std::uint64_t thread_exit_list);
std::array<std::byte,8192> serialize_middle_node_registry(const NativeAtexitBss& registry,bool live_removed);
}
