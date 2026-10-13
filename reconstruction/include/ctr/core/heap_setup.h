// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <span>
#include <cstddef>
#include <cstdint>
namespace ctr { std::uintptr_t heap_setup(std::span<std::byte> memory, std::uintptr_t identity, std::uint64_t length); }
