#pragma once
#include "ctr/metaobject_registration_name.h"
namespace ctr {
MetaobjectRegistrationName register_named_generated_internal_object_base(
 std::span<std::byte> descriptor,std::span<const std::byte> manager,
 std::span<std::byte> strings,const SystemPoolAllocation& names_table,
 std::span<std::byte> pool,SystemPoolBackend& backend,bool poison,
 std::uint64_t& cell,const SystemPoolAllocation& descriptor_array);
}
