#pragma once
#include "ctr/pooled_name_insert.h"
namespace ctr {
// Full015a74c0 first insert on cold-created/initialized1024-bucket context.
PooledNameInsertion insert_first_created_name(std::span<std::byte> object,
 const SystemPoolAllocation& table,std::span<std::byte> pool,
 SystemPoolBackend& backend,std::string_view name,bool poison);
}
