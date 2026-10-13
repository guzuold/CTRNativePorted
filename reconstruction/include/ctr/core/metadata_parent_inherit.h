// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <span>
#include <functional>
#include <cstdint>
#include <cstddef>
namespace ctr {
using MetadataReserveOriginal=std::function<void(std::uintptr_t,std::int32_t)>;
void metadata_parent_inherit(std::span<std::byte,152> child,std::byte* parent,
 bool include_all,const MetadataReserveOriginal& reserve);
}
