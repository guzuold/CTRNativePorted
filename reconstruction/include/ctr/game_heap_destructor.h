#pragma once
#include "ctr/system_pool_backend.h"
#include <array>
namespace ctr {
// Distinct bounded two-argument service; valid exclusive mapped owner only.
int release_direct_memory_region(NativeDirectMemory& owner,std::int64_t physical,std::uint64_t length);
struct GameHeapDestructorSnapshot {
 std::uint64_t published_manager{};
 std::array<std::byte,16384> backing;
 std::array<std::byte,32> manager;
 std::array<std::byte,256> stack;
 std::array<std::uint64_t,16> registers;
};
struct GameHeapDestructorResult {
 std::array<GameHeapDestructorSnapshot,3> before_release;
 std::uint64_t published_manager{};
 std::array<std::byte,32> manager;
 std::array<std::byte,256> stack;
 std::array<std::uint64_t,16> registers;
};
// Original callback01437910, exclusive backend, no publication/readers/atexit invocation.
GameHeapDestructorResult destroy_registered_game_heap(SystemPoolBackend& backend,
 std::span<std::byte> manager,const std::array<std::uint64_t,16>& entry,
 std::span<std::byte> stack);
// Bounded original __cxa_finalize CALL r13 caller frame; callback return word is at RSP.
GameHeapDestructorResult destroy_registered_game_heap_from_finalizer(SystemPoolBackend& backend,
 std::span<std::byte> manager,const std::array<std::uint64_t,16>& entry,
 std::span<std::byte> stack);
}
