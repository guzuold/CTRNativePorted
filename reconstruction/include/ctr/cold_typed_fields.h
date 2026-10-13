#pragma once
#include "ctr/system_pool_allocate.h"
namespace ctr {
enum class ColdFieldKind : std::uint32_t { UnsignedShort, ObjectReference, RawReference, Vector };
SystemPoolAllocation create_cold_typed_field(std::span<const std::byte> descriptor,
 std::span<const std::byte> manager,std::span<std::byte> pool,SystemPoolBackend& backend,
 ColdFieldKind kind,bool poison);
}
