// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_registry_existing.h"
#include "ctr/core/metadata_registry_lookup_linear.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint32_t metadata_registry_existing(std::span<const std::byte> m,std::uintptr_t base,std::size_t metadata){
 if(metadata>m.size()||0x18>m.size()-metadata)throw std::runtime_error("Missing bounded metadata");
 std::uintptr_t query;std::memcpy(&query,m.data()+metadata+0x10,8);
 if(!metadata_registry_lookup_linear(m,base,query))throw std::runtime_error("Missing original registry insertion");
 return 1;
}
}
