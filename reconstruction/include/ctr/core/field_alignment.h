// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <span>
#include <cstddef>
#include <cstdint>
namespace ctr {
std::uint32_t field_alignment_default8(std::span<std::byte,36> field);
}
