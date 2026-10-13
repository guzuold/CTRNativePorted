// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstddef>
#include <span>
namespace ctr { void reference_append(std::span<std::byte,24> header,std::span<const std::uintptr_t> references); }
