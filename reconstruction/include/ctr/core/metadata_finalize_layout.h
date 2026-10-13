// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <span>
#include <cstddef>
#include <cstdint>
#include <functional>
namespace ctr {
// Provider resolves the object's actual virtual slot and invokes original SysV code.
using MetadataOriginalVirtual = std::function<std::uint64_t(std::uintptr_t,std::size_t,std::uintptr_t)>;
void metadata_finalize_layout(std::span<std::byte,128> metadata,
 const std::byte* identifier_registry=nullptr,const MetadataOriginalVirtual* original_virtual=nullptr);
}
