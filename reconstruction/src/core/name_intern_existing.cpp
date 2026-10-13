// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/name_intern_existing.h"
#include <atomic>
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uintptr_t name_intern_existing(std::span<std::byte> m,std::uintptr_t base,std::uintptr_t query){
 if(!query)return 0;
 auto offset=[&](std::uintptr_t p,std::size_t n){if(p<base||p-base>m.size()||n>m.size()-(p-base))throw std::runtime_error("Missing intern extent");return static_cast<std::size_t>(p-base);};
 auto q=[&](std::size_t o){std::uintptr_t v;offset(base+o,8);std::memcpy(&v,m.data()+o,8);return v;};
 auto string=[&](std::uintptr_t p){auto o=offset(p,1);auto end=static_cast<const std::byte*>(std::memchr(m.data()+o,0,m.size()-o));if(!end)throw std::runtime_error("Missing bounded intern string");return reinterpret_cast<const char*>(m.data()+o);};
 offset(base,0x29);if(m[0x28]!=std::byte{})throw std::runtime_error("Missing synchronized name intern");
 auto text=string(query);std::uint32_t hash=0x811c9dc5;for(auto p=reinterpret_cast<const unsigned char*>(text);*p;++p)hash=(hash^*p)*0x1000193u;
 std::uint32_t count;std::memcpy(&count,m.data()+0x18,4);if(!count||count>128)throw std::runtime_error("Missing bounded name buckets");
 auto buckets=offset(q(0x10),static_cast<std::size_t>(count)*8);auto node=q(buckets+(hash%count)*8);
 for(unsigned steps=0;node&&steps<16;++steps){auto o=offset(node,17);if(std::strcmp(text,string(node+16))==0){
  if(reinterpret_cast<std::uintptr_t>(m.data()+o+8)%alignof(std::uint32_t))throw std::runtime_error("Missing aligned name reference");
  std::atomic_ref<std::uint32_t>(*reinterpret_cast<std::uint32_t*>(m.data()+o+8)).fetch_add(1,std::memory_order_seq_cst);return node+16;
 }node=q(o);}
 throw std::runtime_error("Missing original name allocation or unbounded chain");
}
}
