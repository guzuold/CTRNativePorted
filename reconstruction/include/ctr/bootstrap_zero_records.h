#pragma once
#include "ctr/bootstrap_descriptor_arrays.h"
namespace ctr {
// Allocation/zero/publication only, before metadata call01baba3c.
SystemPoolAllocation create_bootstrap_zero_record(std::span<std::byte> descriptor,
 std::span<std::byte> pool,SystemPoolBackend& backend,bool poison,unsigned index);
}
