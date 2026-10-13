#pragma once
#include "ctr/fresh_field_backing.h"
namespace ctr {
// Explicit token/object binding; fresh Vector descriptor only, single thread.
SystemPoolAllocation set_fresh_vector_descriptor_default(std::span<std::byte> field,
 std::span<const std::byte> manager,std::span<std::byte> pool,SystemPoolBackend& backend,
 bool poison,std::uint64_t new_token,std::span<std::byte> new_object);
}
