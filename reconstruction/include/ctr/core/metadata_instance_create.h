// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <functional>
#include <cstdint>
namespace ctr {
using OriginalIntegerCall2=std::function<std::uintptr_t(std::uintptr_t,std::uintptr_t)>;
using OriginalIntegerCall3=std::function<std::uintptr_t(std::uintptr_t,std::uintptr_t,std::uint32_t)>;
std::uintptr_t metadata_instance_create(std::uintptr_t source,std::uintptr_t context,
 std::uint32_t mode,const OriginalIntegerCall2& type_query,
 const OriginalIntegerCall3& factory,const OriginalIntegerCall2& initialize,
 const OriginalIntegerCall2& initialize_alternate);
}
