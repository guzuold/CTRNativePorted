#pragma once
#include "ctr/descriptor_metadata_registration.h"
namespace ctr {
// Full01bad82c final storage, bounded to prepared descriptor+a8=0.
MetaobjectRegistrationName register_generated_internal_object_base(
 std::span<std::byte> descriptor,std::span<const std::byte> pool_manager,
 std::span<std::byte> strings,std::span<std::byte> buckets,std::uint64_t buckets_address,
 std::span<std::byte> pool,SystemPoolBackend& backend,bool poison,
 std::uint64_t& cell,const SystemPoolAllocation& descriptor_array);
}
