// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <span>
#include <cstddef>
#include <cstdint>
namespace ctr { std::uint32_t metadata_registry_publish_linear(std::span<std::byte> m,std::uintptr_t base,std::size_t metadata,std::size_t registry=0,std::size_t descriptor=1024); }
