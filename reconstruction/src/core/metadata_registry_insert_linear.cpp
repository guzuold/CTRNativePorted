// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_registry_insert_linear.h"
#include <atomic>
#include <cstring>
#include <stdexcept>
namespace ctr {
void metadata_registry_insert_linear(std::span<std::byte> m,std::uintptr_t base,std::size_t metadata,std::size_t registry){
 auto off=[&](std::uintptr_t p,std::size_t size){if(p<base||p-base>m.size()||size>m.size()-(p-base))throw std::runtime_error("Missing bounded registry input");return static_cast<std::size_t>(p-base);};
 auto q=[&](std::uintptr_t p){std::uintptr_t v;std::memcpy(&v,m.data()+off(p,8),8);return v;};
 if(q(base+registry+0x30))throw std::runtime_error("Missing original hash insertion");
 auto list=q(base+registry+0x20);auto entries=q(list+0x10);std::int32_t count;std::memcpy(&count,m.data()+off(list,4),4);
 if(count<0||count>=8||!entries||(q(list+8)>>3)<=static_cast<unsigned>(count))throw std::runtime_error("Missing original registry growth");
 auto dest=off(entries+8*count,8);auto refs=off(base+metadata+8,4);auto* p=reinterpret_cast<std::uint32_t*>(m.data()+refs);
 if(reinterpret_cast<std::uintptr_t>(p)%std::atomic_ref<std::uint32_t>::required_alignment)throw std::runtime_error("Missing aligned references");
 std::atomic_ref<std::uint32_t>(*p).fetch_add(1,std::memory_order_seq_cst);
 auto obj=base+metadata;std::memcpy(m.data()+dest,&obj,8);++count;std::memcpy(m.data()+off(list,4),&count,4);
 std::int32_t mode;std::memcpy(&mode,m.data()+off(base+registry+0x18,4),4);if(!mode){std::uint32_t serial;std::memcpy(&serial,m.data()+off(base+registry+0x64,4),4);++serial;std::memcpy(m.data()+off(base+registry+0x64,4),&serial,4);}
}
}
