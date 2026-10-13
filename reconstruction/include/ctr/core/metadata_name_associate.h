// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <span>
#include <functional>
#include <cstddef>
#include <cstdint>
namespace ctr {
using MetadataNameOriginal=std::function<std::uintptr_t(std::uintptr_t,std::uintptr_t)>;
void metadata_name_associate(std::span<std::byte,24> metadata,
 std::uintptr_t name_context,const char* name,
 const MetadataNameOriginal& intern,const MetadataNameOriginal& release);
}
