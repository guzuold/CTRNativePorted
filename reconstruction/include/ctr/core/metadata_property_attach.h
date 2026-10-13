// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
namespace ctr {
void metadata_property_attach(std::span<const std::byte,48> metadata,std::uintptr_t cached_type,
 const std::function<std::uintptr_t(std::uintptr_t)>& query,
 const std::function<void(std::uintptr_t)>& callback);
}
