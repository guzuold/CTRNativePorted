#pragma once
#include "ctr/created_name_first_insert.h"
namespace ctr {
struct CreatedNameResult { PooledNameInsertion name; bool created; };
// Original015a74c0, unlocked1024bucket context; every live record explicitly supplied.
CreatedNameResult intern_created_name(std::span<std::byte> object,
 const SystemPoolAllocation& table,std::span<std::byte> pool,
 SystemPoolBackend& backend,std::span<const PooledNameInsertion> records,
 std::string_view text,bool poison);
}
