#pragma once
#include "ctr/metaobject_registration_complete.h"
namespace ctr {
MetaobjectRegistrationName complete_descriptor_metadata_registration(
 std::span<std::byte> descriptor,std::span<const std::byte> pool_manager,
 std::span<std::byte> strings,std::span<std::byte> buckets,std::uint64_t buckets_address,
 std::span<std::byte> pool,SystemPoolBackend& backend,std::string_view name,
 std::uint32_t instance_size,bool poison,std::uint64_t& cell,
 const SystemPoolAllocation& descriptor_array);
}
