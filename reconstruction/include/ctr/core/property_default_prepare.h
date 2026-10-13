// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
namespace ctr {
using PropertyOriginal=std::function<std::uint64_t(std::uintptr_t,std::size_t,std::uintptr_t)>;
void property_default_prepare(std::span<const std::byte,80> property,
 const std::function<std::uintptr_t(std::uintptr_t)>& lookup,const PropertyOriginal& original,
 const std::function<void(std::uintptr_t,std::uintptr_t)>& deserialize,
 const std::function<void(std::uintptr_t,std::uintptr_t)>& initialize);
}
