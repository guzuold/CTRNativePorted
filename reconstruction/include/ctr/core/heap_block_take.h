// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <span>
#include <cstddef>
#include <cstdint>
namespace ctr {std::uintptr_t heap_block_take(std::span<std::byte> block,std::uintptr_t identity,std::uint64_t requested,std::span<std::byte> heap={},std::uintptr_t heap_identity=0,std::span<std::byte> predecessor={},std::uintptr_t predecessor_identity=0);}
