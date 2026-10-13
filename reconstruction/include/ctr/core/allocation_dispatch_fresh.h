// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <span>
#include <functional>
#include <cstddef>
#include <cstdint>
namespace ctr {using PoolFreshOriginal=std::function<std::uintptr_t(std::uintptr_t,std::uintptr_t,std::uint64_t,std::uint32_t)>;
std::uintptr_t allocation_dispatch_fresh(std::span<std::byte>,std::uintptr_t,std::size_t,std::uintptr_t,std::uint64_t,std::uint32_t,std::uint32_t,std::uint64_t,std::uintptr_t,std::uintptr_t,const PoolFreshOriginal&,std::uintptr_t known_140=0);}
