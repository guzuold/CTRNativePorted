#pragma once
#include "ctr/system_pool_mode1.h"
namespace ctr {
// Actual24-byte array header and128 pointer slots, prepared System Pool/type1.
SystemPoolAllocation create_bootstrap_pointer_array(std::span<std::byte> pool,
 SystemPoolBackend& backend,bool poison);
}
