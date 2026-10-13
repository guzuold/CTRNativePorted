#pragma once
#include "ctr/bootstrap_pointer_array.h"
namespace ctr {
SystemPoolAllocation create_bootstrap_secondary_array(std::span<std::byte> pool,
 SystemPoolBackend& backend,bool poison);
void publish_bootstrap_array(std::span<std::byte> descriptor,
 const SystemPoolAllocation& allocation,bool secondary);
// Bounded caller01bab701..01bab8ce; nonthreaded selector1, large-capacity flag0.
SystemPoolAllocation construct_bootstrap_descriptor_arrays(std::span<std::byte> descriptor,
 SystemPoolBackend& backend,std::span<std::byte> pool,bool poison);
}
