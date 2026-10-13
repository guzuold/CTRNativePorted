// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "allocation_dispatch_fresh.h"
namespace ctr {
std::uintptr_t registration_list_create(std::span<std::byte>,std::uintptr_t,std::size_t,std::int32_t,const PoolFreshOriginal&,const std::function<std::uintptr_t()>& context_lookup={});
}
