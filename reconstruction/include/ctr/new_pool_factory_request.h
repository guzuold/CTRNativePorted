#pragma once
#include "ctr/new_pool_activation_prefix.h"
namespace ctr {
enum class PoolFactoryDependency { IgMutexMetadataInitializerMissing };
struct PendingPoolFactoryRequest {
 SystemPoolAllocation manager;
 std::uint64_t metadata_cell;
 std::uint64_t initializer;
 std::uint64_t pool;
 PoolFactoryDependency missing=PoolFactoryDependency::IgMutexMetadataInitializerMissing;
};
// Stop before014ecc80 initializer; result is pending, never a created mutex.
PendingPoolFactoryRequest begin_new_pool_factory_request(std::span<std::byte> descriptor,
 std::span<const std::byte> previous_manager,std::span<std::byte> previous_pool,
 SystemPoolBackend& backend,bool poison,std::uint64_t& publication);
}
