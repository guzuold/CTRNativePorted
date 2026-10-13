#pragma once
#include "ctr/mspace_layout.h"
#include "ctr/native_mutex.h"
#include <string_view>
namespace ctr {
struct MspaceCreateContext {
    bool parent_tracking{};
    std::uint8_t lock_policy{};
    std::uint64_t* bitmap{};
    std::span<std::uint64_t> table;
};
// Original libc +603df through real return; named, supplied backing, flags/options zero.
// Global mutex/debug and time tracking disabled by explicit caller contract.
std::uint64_t create_named_mspace(std::span<std::byte> backing,
    const MspaceLayoutParameters& parameters,std::string_view name,const MspaceCreateContext& context);
struct MspaceCreation {std::uint64_t header_token{};std::unique_ptr<NativeMutex> mutex;};
MspaceCreation create_named_mspace_with_mutex(std::span<std::byte> backing,
    const MspaceLayoutParameters& parameters,std::string_view name,const MspaceCreateContext& context);
struct MspaceGlobalConfiguration {
    std::uint8_t poison{},page_scale{},granularity_scale{},lock_policy{},debug_flags{};
};
// Original create export, flags/options zero, supplied backing and explicit runtime globals.
MspaceCreation create_default_named_mspace(std::span<std::byte> backing,std::uint64_t base_token,
    std::uint64_t capacity,std::string_view name,MspaceGlobalConfiguration& globals,
    const MspaceCreateContext& context);
}
