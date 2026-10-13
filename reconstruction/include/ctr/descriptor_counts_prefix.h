#pragma once
#include "ctr/bootstrap_descriptor_arrays.h"
namespace ctr {
// 01bac592..01bac5cb, excluding the following kernel call.
void update_bootstrap_descriptor_counts(std::span<std::byte> descriptor,
 const SystemPoolAllocation& registry_array,std::span<std::byte> pool,
 SystemPoolBackend& backend,bool poison,std::uint64_t& lazy_cell,
 SystemPoolAllocation& lazy_array);
}
