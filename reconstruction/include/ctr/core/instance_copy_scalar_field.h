// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <span>
#include <cstddef>
namespace ctr {
void instance_copy_scalar_field(std::span<const std::byte,24> field,
 std::span<std::byte> destination,std::span<const std::byte> source);
}
