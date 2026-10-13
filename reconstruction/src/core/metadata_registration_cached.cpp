// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_registration_cached.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint64_t metadata_registration_cached(std::span<const std::byte> m,std::uintptr_t base,std::uintptr_t slot){
 auto offset=[&](std::uintptr_t p,std::size_t n){if(p<base||p-base>m.size()||n>m.size()-(p-base))throw std::runtime_error("Missing bounded registration metadata");return static_cast<std::size_t>(p-base);};
 std::uintptr_t metadata;std::memcpy(&metadata,m.data()+offset(slot,8),8);if(!metadata)throw std::runtime_error("Missing original metadata creation");
 std::uint16_t flags;std::memcpy(&flags,m.data()+offset(metadata+0x5c,2),2);if(!(flags&8))throw std::runtime_error("Missing original registration callbacks/creation");return 0;
}
}
