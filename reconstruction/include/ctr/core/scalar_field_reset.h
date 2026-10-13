#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
// Whole 014ddb10 in the known scalar/raw +140 domain. Other virtual reset
// providers, libc fallbacks and object/vector ownership are declared missing.
void reset_prepared_scalar_field(std::span<const std::byte> field,
    std::span<std::byte> object, std::span<const std::byte> default_backing,
    std::uint64_t default_backing_token);
}
