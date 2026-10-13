// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <span>
#include <cstddef>
#include <cstdint>
namespace ctr { void heap_region_publish(std::span<std::byte> memory, std::uintptr_t identity, std::size_t header, std::uint64_t length); }
