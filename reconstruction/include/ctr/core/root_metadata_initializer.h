// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "metadata_registration_cold.h"
#include "allocation_dispatch_fresh.h"
namespace ctr {
// Bounded unsynchronized zero-field bootstrap; providers must execute verified
// allocation/constructor/name behavior. Publication slots are resolved fixture views.
std::uintptr_t root_metadata_initializer(std::span<std::byte>,std::uintptr_t,
 std::size_t root_slot,std::size_t base_slot,const MetadataNameOriginal&,
 const std::function<std::uintptr_t()>& create,const std::function<std::uintptr_t()>& list,
 const PoolFreshOriginal& allocate);
}
