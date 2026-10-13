#pragma once
#include "ctr/metaobject_allocation_prefix.h"
namespace ctr {
enum class MetaobjectFactoryState { ColdObjectCreated, WarmMetadataFactoryMissing };
struct MetaobjectFactoryDispatch {
 MetaobjectFactoryState state;
 SystemPoolAllocation allocation;
 std::uint64_t metadata_cell;
 std::uint64_t initializer;
 std::uint64_t pool;
};
// Full cold return or pending warm boundary0156bb5a, never factory success stub.
MetaobjectFactoryDispatch dispatch_metaobject_creation(std::span<const std::byte> descriptor,
 std::span<const std::byte> manager,std::span<std::byte> pool,SystemPoolBackend& backend,bool poison);
}
