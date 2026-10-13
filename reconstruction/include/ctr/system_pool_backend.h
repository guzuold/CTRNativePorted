#pragma once
#include "ctr/native_direct_memory.h"
#include "ctr/mspace_create.h"
#include "ctr/pool_activation.h"
namespace ctr {
struct SystemPoolBackend {
 // Destruction order: mutex/heap first, Windows backing last.
 NativeDirectMemory memory;
 MspaceCreation heap;
 std::uint64_t capacity{},original_descriptor_identity{},original_manager_identity{};
 std::int64_t physical{};
 std::uint8_t initialized_flag{};
// Prepared pool storage, declared early registry lifecycle; binds fields to real backend resources.
std::uint32_t activate_owned_system_pool(std::span<std::byte> pool,SystemPoolBackend& backend,
 const SystemPoolBootstrapContext& lifecycle);
// Original factory postallocation section only; allocated pool and prepared nonthreaded manager.
std::uint32_t construct_allocated_system_pool(std::span<std::byte> pool,
 std::span<std::byte> manager,SystemPoolBackend& backend);
};
// Original01537820 successful preconfigured-capacity path; identities are NOT callable native vtables.
void initialize_preconfigured_system_pool(SystemPoolBackend& backend,std::uint64_t capacity,
 MspaceGlobalConfiguration& globals);
// Prepared pool storage, declared early registry lifecycle; binds fields to real backend resources.
std::uint32_t activate_owned_system_pool(std::span<std::byte> pool,SystemPoolBackend& backend,
 const SystemPoolBootstrapContext& lifecycle);
// Original factory postallocation section only; allocated pool and prepared nonthreaded manager.
std::uint32_t construct_allocated_system_pool(std::span<std::byte> pool,
 std::span<std::byte> manager,SystemPoolBackend& backend);
}
