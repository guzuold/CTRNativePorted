// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstddef>
#include <span>
namespace ctr {
void field_default_initialize(std::span<const std::byte,64> field,std::span<std::byte> object);
}
