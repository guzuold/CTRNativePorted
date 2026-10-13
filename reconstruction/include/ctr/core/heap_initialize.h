// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <span>
#include <cstddef>
#include <cstdint>
namespace ctr { std::uintptr_t heap_initialize(std::span<std::byte> heap, std::uintptr_t identity); }
