#pragma once
#include "ctr/descriptor_pool_selection.h"
#include "ctr/system_pool_mode1.h"
namespace ctr {
// Full01539610 warm selector1 continuation, actual constructed System Pool only.
SystemPoolAllocation allocate_warm_descriptor_manager(std::span<const std::byte> descriptor,
 std::span<const std::byte> manager,std::span<std::byte> system_pool,
 SystemPoolBackend& backend,bool poison);
}
