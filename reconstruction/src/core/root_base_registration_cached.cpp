// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/root_base_registration_cached.h"
#include "ctr/core/metadata_registration_cached.h"
namespace ctr {
std::uint64_t root_base_registration_cached(std::span<const std::byte> m,std::uintptr_t base,std::uintptr_t slot){
 // Retail initializer passes the original master its static publication slot.
 // Only its already-in-progress branch is verified here; cold bootstrap is Missing.
 return metadata_registration_cached(m,base,slot);
}
}
