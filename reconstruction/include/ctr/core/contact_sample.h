// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace ctr {
std::uint32_t contact_sample(std::span<const std::byte,112> geometry, float parameter, std::span<float,3> output);
}
