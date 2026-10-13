#pragma once
#include <cstddef>
#include <span>
namespace ctr {
// Entire 014dc920 call, scoped to equal base/derived field counts and original
// vtables whose +20/+40/+50 bodies have been verified as RET instructions.
// Field construction/reset queries and other object lifecycle hooks are missing.
void construct_zero_field_object(std::span<std::byte> object,
    std::span<std::byte> metadata, std::span<const std::byte> descriptor,
    std::span<const std::byte> base_metadata, std::span<const std::byte> pool);
}
