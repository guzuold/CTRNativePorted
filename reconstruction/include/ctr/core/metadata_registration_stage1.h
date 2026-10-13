// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "ctr/core/metadata_name_associate.h"
#include <span>
#include <cstddef>
#include <cstdint>
namespace ctr { std::uint64_t metadata_registration_stage1(std::span<std::byte> m,std::uintptr_t base,std::size_t metadata,std::uintptr_t name_context,std::uint16_t size,const MetadataNameOriginal& intern,const char* name=nullptr); }
