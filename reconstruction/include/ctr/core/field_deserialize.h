// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstddef>
#include <span>
namespace ctr { void field_deserialize(std::span<const std::byte,65> field,std::span<std::byte> object); }
