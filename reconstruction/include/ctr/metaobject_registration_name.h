#pragma once
#include "ctr/metaobject_registration_base.h"
#include "ctr/pooled_name_insert.h"
namespace ctr {
PooledNameInsertion assign_fresh_metaobject_name(std::span<std::byte> metadata,
 std::span<std::byte> strings,std::span<std::byte> buckets,std::uint64_t buckets_address,
 std::span<std::byte> pool,SystemPoolBackend& backend,std::string_view name,bool poison);
struct MetaobjectRegistrationName {
 MetaobjectRegistrationBase registration;
 PooledNameInsertion name;
};
// Prepared null parent/type/callback; stop before0153fdd8 parent-field scan.
MetaobjectRegistrationName begin_cold_metaobject_registration_name(
 std::span<const std::byte> descriptor,std::span<const std::byte> manager,
 std::span<std::byte> strings,std::span<std::byte> buckets,std::uint64_t buckets_address,
 std::span<std::byte> pool,SystemPoolBackend& backend,std::string_view name,
 std::uint32_t instance_size,bool poison,std::uint64_t& registration_cell);
}
