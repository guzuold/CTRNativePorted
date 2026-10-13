// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/auto/auto_0143cfd0.hpp"
#include "ctr/core/metadata_identifier_lookup.h"
#include <cstring>
namespace ctr {std::uintptr_t auto_0143cfd0(std::span<const std::byte,80> object,std::span<const std::byte,120> registry){std::uint16_t index;std::memcpy(&index,object.data()+56,2);if(index==0xffff)return 0;return metadata_identifier_lookup(registry,index);}}
