// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "ctr/core/metadata_instance_create.h"
#include <span>
#include <cstddef>
namespace ctr {
std::byte* instance_create_reference(std::span<std::byte,8> output,std::uintptr_t source,
 std::uintptr_t context,std::uint32_t mode,const OriginalIntegerCall3& create,
 const OriginalIntegerCall2& release);
}
