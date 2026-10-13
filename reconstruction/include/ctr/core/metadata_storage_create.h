// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "metadata_instance_create.h"
namespace ctr {
std::uintptr_t metadata_storage_create(std::uintptr_t context,bool use_factory,
 std::uintptr_t type_slot,std::uintptr_t type_name,const OriginalIntegerCall2& type_lookup,
 const OriginalIntegerCall3& factory,const OriginalIntegerCall2& allocate,
 const OriginalIntegerCall2& construct,const OriginalIntegerCall2& initialize);
}
