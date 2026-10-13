#pragma once
#include "ctr/created_name_first_insert.h"
namespace ctr {
// Full015a74c0 first insertion: active object, free lock and positive spin budget.
PooledNameInsertion insert_first_activated_name(std::span<std::byte> object,
 const SystemPoolAllocation& table,std::span<std::byte> pool,
 SystemPoolBackend& backend,std::string_view name,bool poison,std::int32_t spin_limit);
}
