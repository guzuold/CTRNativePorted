#pragma once
#include "ctr/named_generated_root.h"
#include "ctr/descriptor_counts_prefix.h"
namespace ctr {
// Generated root plus01bac592..01bac5cb; stop before sceUserServiceInitialize.
MetaobjectRegistrationName register_named_root_and_counts(
 std::span<std::byte> descriptor,std::span<const std::byte> manager,
 std::span<std::byte> strings,const SystemPoolAllocation& names_table,
 std::span<std::byte> pool,SystemPoolBackend& backend,bool poison,
 std::uint64_t& cell,const SystemPoolAllocation& registry_array,
 std::uint64_t& lazy_cell,SystemPoolAllocation& lazy_array);
}
