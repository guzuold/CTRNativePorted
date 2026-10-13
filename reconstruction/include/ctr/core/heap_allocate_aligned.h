// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "heap_allocate.h"
namespace ctr {std::uintptr_t heap_allocate_aligned(std::span<std::byte> memory,std::uintptr_t identity,std::size_t heap_offset,std::uint64_t alignment,std::uint64_t requested);}
