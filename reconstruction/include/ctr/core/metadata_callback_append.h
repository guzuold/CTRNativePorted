// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <span>
#include <cstddef>
#include <cstdint>
namespace ctr { void metadata_callback_append(std::span<std::byte,16> list, std::uintptr_t callback); }
