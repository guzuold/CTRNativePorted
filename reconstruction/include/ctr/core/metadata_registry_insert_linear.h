// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <span>
#include <cstddef>
#include <cstdint>
namespace ctr { void metadata_registry_insert_linear(std::span<std::byte> m,std::uintptr_t base,std::size_t metadata,std::size_t registry=0); }
