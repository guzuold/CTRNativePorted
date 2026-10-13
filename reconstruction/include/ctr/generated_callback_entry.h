#pragma once
#include "ctr/generated_callback_continuation.h"
namespace ctr {
ColdFieldTable execute_generated_metadata_callback(std::span<std::byte> metadata,
 std::span<const std::byte> descriptor,std::span<const std::byte> manager,
 std::span<std::byte> pool,SystemPoolBackend& backend,std::span<std::byte> names,
 const SystemPoolAllocation& buckets,std::span<const std::byte> cached_types,
 std::span<const std::byte> canonical,std::uint64_t metadata_token,std::uint64_t canonical_token,
 std::uint64_t element_type,std::uint64_t attribute_type,std::uint64_t base_type,
 std::span<std::byte> publications,bool poison);
}
