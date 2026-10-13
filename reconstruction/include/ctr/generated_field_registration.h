#pragma once
#include "ctr/cold_field_table.h"
#include "ctr/generated_metadata_names.h"
namespace ctr {
// Explicit cache bindings ordered Int,UShort,ObjectRef,RawRef,Vector; actual registry not supplied.
void register_generated_fields(ColdFieldTable& fields,std::span<std::byte> metadata,
 std::span<std::byte> names,const SystemPoolAllocation& buckets,std::span<std::byte> pool,
 SystemPoolBackend& backend,std::span<const std::byte> cached_types,
 std::span<const std::uint16_t,16> offsets,std::span<std::byte> publications,bool poison);
}
