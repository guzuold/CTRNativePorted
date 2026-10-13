#pragma once
#include "ctr/metaobject_registration_name.h"
namespace ctr {
MetaobjectRegistrationName complete_prepared_metaobject_registration(
 std::span<const std::byte> descriptor,std::span<const std::byte> pool_manager,
 std::span<std::byte> strings,std::span<std::byte> buckets,std::uint64_t buckets_address,
 std::span<std::byte> pool,SystemPoolBackend& backend,std::string_view name,
 std::uint32_t instance_size,bool poison,std::uint64_t& cell,
 std::span<std::byte> registry,std::span<std::byte> header,std::uint64_t header_token,
 std::span<std::byte> table,std::uint64_t table_token);
}
