#pragma once
#include "ctr/native_heap_api_registry.h"
namespace ctr {
// Explicit guest-table -> owned callable host-table translation for import4c8.
// Only one publication; original kernel replacement/error ABI remains unverified.
std::shared_ptr<const NativeGameHeapApi> publish_game_heap_api(
 NativeHeapApiRegistry&,std::shared_ptr<SystemPoolBackend>,
 const GameHeapApiContinuation&,std::span<const std::byte> slots,
 std::uint64_t original_table_argument,bool poison);
}
