// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <span>
#include <cstddef>
#include <cstdint>
namespace ctr { std::uintptr_t metadata_registry_lookup_linear(std::span<const std::byte> m,std::uintptr_t base,std::uintptr_t query,std::size_t registry=0,std::size_t descriptor=1024); }
