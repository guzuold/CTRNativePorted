// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "heap_allocate_aligned.h"
#include <functional>
namespace ctr {using PoolAlignedOriginal=std::function<std::uintptr_t(std::uintptr_t,std::uint64_t,std::uint64_t)>;std::uintptr_t pool_direct_aligned_allocate(std::span<std::byte>,std::uintptr_t,std::size_t,std::uint64_t,std::uint32_t,const PoolAlignedOriginal& original={});}
