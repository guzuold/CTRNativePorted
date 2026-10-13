#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
struct SystemPoolBootstrapContext {
    bool descriptor_initialized{};
    bool threaded_manager{};
    std::int32_t tls_key{-1};
    std::uint64_t backing_size{}, backing_token{}, allocator_token{};
};
// 014d7c30 with original system-pool table 026b9ca8. Opaque tokens, no allocator.
std::uint32_t activate_bootstrap_system_pool(std::span<std::byte> storage,
    const SystemPoolBootstrapContext& context);
}
