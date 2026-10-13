// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/name_intern_allocate.h"
#include <atomic>
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uintptr_t name_intern_allocate(std::span<std::byte> m,std::uintptr_t base,std::size_t context,std::uintptr_t query,const PoolFreshOriginal& allocate){
 if(!query)return 0;
 auto offset=[&](std::uintptr_t p,std::size_t n){if(p<base||p-base>m.size()||n>m.size()-(p-base))throw std::runtime_error("Missing intern extent");return static_cast<std::size_t>(p-base);};
 auto q=[&](std::size_t o){std::uintptr_t v;offset(base+o,8);std::memcpy(&v,m.data()+o,8);return v;};
 auto string=[&](std::uintptr_t p){auto o=offset(p,1);auto end=static_cast<const std::byte*>(std::memchr(m.data()+o,0,m.size()-o));if(!end)throw std::runtime_error("Missing bounded intern string");return reinterpret_cast<const char*>(m.data()+o);};
 offset(base+context,0x40);if(m[context+0x28]!=std::byte{})throw std::runtime_error("Missing synchronized name intern");
 auto text=string(query);std::uint32_t hash=0x811c9dc5;for(auto p=reinterpret_cast<const unsigned char*>(text);*p;++p)hash=(hash^*p)*0x1000193u;
 std::uint32_t count;std::memcpy(&count,m.data()+context+0x18,4);if(!count||count>128)throw std::runtime_error("Missing bounded name buckets");
 auto buckets=offset(q(context+0x10),static_cast<std::size_t>(count)*8);auto node=q(buckets+(hash%count)*8);
 for(unsigned steps=0;node&&steps<16;++steps){auto o=offset(node,17);if(std::strcmp(text,string(node+16))==0){
  if(reinterpret_cast<std::uintptr_t>(m.data()+o+8)%alignof(std::uint32_t))throw std::runtime_error("Missing aligned name reference");
  std::atomic_ref<std::uint32_t>(*reinterpret_cast<std::uint32_t*>(m.data()+o+8)).fetch_add(1,std::memory_order_seq_cst);return node+16;
 }node=q(o);}
 if(node)throw std::runtime_error("Missing bounded name chain");
 auto length=std::strlen(text)+1;if(length>0xffffffffu)throw std::runtime_error("Missing bounded name length");
 auto po=offset(q(context+0x38),0xa1);auto saved=m[po+0xa0];m[po+0xa0]=std::byte{};
 std::uint32_t alignment;std::memcpy(&alignment,m.data()+po+0x30,4);
 auto allocated=allocation_dispatch_fresh(m,base,po,0,length+16,alignment,0,0,0x4013d7fe0,0x4013db3c0,allocate,0x4013db610);m[po+0xa0]=saved;
 auto o=offset(allocated,length+16);std::uint32_t one=1;std::atomic_ref<std::uint32_t>(*reinterpret_cast<std::uint32_t*>(m.data()+o+8)).exchange(one,std::memory_order_seq_cst);
 std::uintptr_t zero=0;std::memcpy(m.data()+o,&zero,8);std::memcpy(m.data()+o+16,text,length);
 auto bucket=buckets+(hash%count)*8;auto head=q(bucket);std::memcpy(m.data()+o,&head,8);std::memcpy(m.data()+bucket,&allocated,8);
 std::uint32_t used,watermark;std::memcpy(&used,m.data()+context+0xc,4);std::memcpy(&watermark,m.data()+context+0x1c,4);++used;std::memcpy(m.data()+context+0xc,&used,4);if(used>watermark)std::memcpy(m.data()+context+0x1c,&used,4);return allocated+16;
}
}
