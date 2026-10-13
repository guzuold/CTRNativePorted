// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_parent_inherit.h"
#include "ctr/metaobject_field_append.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {
template<class T>T get(const std::byte*p){T x{};std::memcpy(&x,p,sizeof(x));return x;}
template<class T>void put(std::byte*p,T x){std::memcpy(p,&x,sizeof(x));}
}
void metadata_parent_inherit(std::span<std::byte,152> child,std::byte* parent,
 bool include_all,const MetadataReserveOriginal& reserve){
 if(!parent)return;
 auto*c=child.data();auto head=get<std::uintptr_t>(parent+0x48);
 if(head){auto*h=reinterpret_cast<std::byte*>(head);put(c+0x50,get<std::uintptr_t>(h+0x50));put(h+0x50,reinterpret_cast<std::uintptr_t>(c));}
 else put(c+0x50,reinterpret_cast<std::uintptr_t>(c));
 put(parent+0x48,reinterpret_cast<std::uintptr_t>(c));put(c+0x40,reinterpret_cast<std::uintptr_t>(parent));
 put(c+0x38,get<std::uintptr_t>(parent+0x38));put(c+0x5e,get<std::uint16_t>(parent+0x5e));put(c+0x5a,get<std::uint16_t>(parent+0x5a));
 auto count=get<std::int32_t>(parent+0x18);auto own=get<std::int32_t>(c+0x18);
 std::uint64_t capacity=0;if(get<std::uintptr_t>(c+0x28))capacity=(get<std::uint64_t>(c+0x20)>>3)&0xffffffffu;
 auto requested=static_cast<std::int32_t>(static_cast<std::uint32_t>(own)+static_cast<std::uint32_t>(count));
 if(requested<static_cast<std::int64_t>(capacity))requested=static_cast<std::int32_t>(capacity);
 if(!reserve)throw std::runtime_error("Missing original reservef9e120");
 reserve(reinterpret_cast<std::uintptr_t>(c+0x18),requested);
 if(count<=0)return;
 auto table=get<std::uintptr_t>(parent+0x28);if(!table)throw std::runtime_error("Missing parent fields");
 for(std::int32_t i=0;i<count;++i){
  auto token=get<std::uintptr_t>(reinterpret_cast<const std::byte*>(table)+static_cast<std::size_t>(i)*8);
  if(!token)throw std::runtime_error("Missing valid inherited descriptor");auto*f=reinterpret_cast<std::byte*>(token);
  if(!include_all&&(get<std::uint16_t>(f+0x22)&0x180))continue;
  auto backing=get<std::uintptr_t>(c+0x28);if(!backing)throw std::runtime_error("Missing child backing");
  append_metaobject_field_no_growth(child,std::span<std::byte>(reinterpret_cast<std::byte*>(backing),64),backing,std::span<std::byte>(f,32),token);
 }
}
}
