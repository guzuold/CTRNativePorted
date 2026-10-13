#pragma once
#include "ctr/derived_pool_init.h"
#include "ctr/pool_activation.h"
namespace ctr {
std::uint32_t initialize_derived_resource_backing(std::span<std::byte> derived,
 std::span<std::byte> parent,SystemPoolBackend& backend,std::uint64_t capacity,
 std::uint32_t alignment,bool poison);
std::uint32_t activate_derived_bitmap_resource(std::span<std::byte> derived,
 SystemPoolBackend& backend,const SystemPoolBootstrapContext& context);
// Bounded original factory type store and virtual+b0 call; not full factory.
std::uint32_t publish_derived_bitmap_resource(std::span<std::byte> derived,
 SystemPoolBackend& backend,const SystemPoolBootstrapContext& context);
}
