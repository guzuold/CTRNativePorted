// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
#include <cstddef>
#include <span>
namespace ctr {std::uintptr_t heap_allocate(std::span<std::byte> memory,std::uintptr_t identity,std::size_t heap_offset,std::uint64_t requested);}
