#pragma once
#include "ctr/metaobject_registration_prefix.h"
#include "ctr/metaobject_base_context.h"
namespace ctr {
struct MetaobjectRegistrationBase {
 SystemPoolAllocation metadata;
 SystemPoolAllocation base_record;
};
// Null parent; actual base01543420; stops BEFORE0153fd83 pooled-name call.
MetaobjectRegistrationBase begin_cold_metaobject_registration_base(
 std::span<const std::byte> descriptor,std::span<const std::byte> manager,
 std::span<std::byte> pool,SystemPoolBackend& backend,bool poison,
 std::uint64_t& registration_cell);
}
