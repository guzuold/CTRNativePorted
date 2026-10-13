// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_registry_lookup_linear.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uintptr_t metadata_registry_lookup_linear(std::span<const std::byte> m,std::uintptr_t base,std::uintptr_t query,std::size_t registry,std::size_t descriptor){
 auto off=[&](std::uintptr_t p,std::size_t size){if(p<base||p-base>m.size()||size>m.size()-(p-base))throw std::runtime_error("Missing bounded registry input");return static_cast<std::size_t>(p-base);};
 auto q=[&](std::uintptr_t p){std::uintptr_t v;std::memcpy(&v,m.data()+off(p,8),8);return v;};
 auto str=[&](std::uintptr_t p){auto at=off(p,1);auto* s=reinterpret_cast<const char*>(m.data()+at);if(!std::memchr(s,0,m.size()-at))throw std::runtime_error("Missing bounded name");return s;};
 if(registry>=m.size())throw std::runtime_error("Missing registry");if(m[registry]==std::byte{0})return 0;
 if(q(base+registry+0x30)||q(base+registry+0x70))throw std::runtime_error("Missing hash registry/synchronization");
 auto list=q(base+registry+0x20);std::int32_t count;std::memcpy(&count,m.data()+off(list,4),4);if(count<0||count>16)throw std::runtime_error("Missing bounded registry list");
 auto entries=q(list+0x10);std::uint16_t nameoff;std::memcpy(&nameoff,m.data()+off(base+descriptor+0x14,2),2);
 for(std::int32_t i=0;i<count;++i){auto obj=q(entries+8*i);auto name=q(obj+nameoff);if(name&&query&&std::strcmp(str(name),str(query))==0)return obj;}return 0;
}
}

