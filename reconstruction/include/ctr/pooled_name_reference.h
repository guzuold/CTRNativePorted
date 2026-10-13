#pragma once
#include "ctr/pooled_name_insert.h"
namespace ctr {
// Single known existing record,1024 buckets, lock disabled; no collision/creation.
std::uint64_t retain_existing_created_name(std::span<const std::byte> context,
 const SystemPoolAllocation& table,const PooledNameInsertion& name,std::string_view text);
//0153f260 first-field segment through0153f43e, before unknown +c0 query.
void bind_first_registered_scalar_field_name(std::span<std::byte> field,
 std::span<const std::byte> metadata,std::span<const std::byte> context,
 const SystemPoolAllocation& table,const PooledNameInsertion& name,
 std::string_view text,std::uint16_t offset);
}
