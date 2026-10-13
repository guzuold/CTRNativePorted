// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_registry_publish_linear.h"
#include "ctr/core/metadata_registry_lookup_linear.h"
#include "ctr/core/metadata_registry_insert_linear.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint32_t metadata_registry_publish_linear(std::span<std::byte> m,std::uintptr_t base,std::size_t metadata,std::size_t registry,std::size_t descriptor){
 if(metadata>m.size()||0x5a>m.size()-metadata)throw std::runtime_error("Missing bounded metadata");
 std::uintptr_t query;std::memcpy(&query,m.data()+metadata+0x10,8);
 if(metadata_registry_lookup_linear(m,base,query,registry,descriptor))return 1;
 metadata_registry_insert_linear(m,base,metadata,registry);
 auto off=[&](std::uintptr_t p,std::size_t size){if(p<base||p-base>m.size()||size>m.size()-(p-base))throw std::runtime_error("Missing bounded registry list");return static_cast<std::size_t>(p-base);};
 std::uintptr_t list,entries;std::memcpy(&list,m.data()+off(base+registry+0x20,8),8);std::memcpy(&entries,m.data()+off(list+0x10,8),8);
 std::int32_t count;std::memcpy(&count,m.data()+off(list,4),4);std::uint16_t index=0;
 for(;index<count;++index){std::uintptr_t obj;std::memcpy(&obj,m.data()+off(entries+index*8,8),8);if(obj==base+metadata)break;}
 std::memcpy(m.data()+metadata+0x58,&index,2);return 0;
}
}
