// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_property_attach.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {template<class T>T read(std::uintptr_t p,std::size_t o){T v{};std::memcpy(&v,reinterpret_cast<const std::byte*>(p)+o,sizeof(v));return v;}}
void metadata_property_attach(std::span<const std::byte,48> metadata,std::uintptr_t cached_type,
 const std::function<std::uintptr_t(std::uintptr_t)>& query,
 const std::function<void(std::uintptr_t)>& callback){
 const auto identity=reinterpret_cast<std::uintptr_t>(metadata.data());
 const auto count=read<std::int32_t>(identity,24);if(count<=0)return;
 if(!query||!callback)throw std::runtime_error("Missing original property type/callback providers");
 auto table=read<std::uintptr_t>(identity,40);if(!table)throw std::runtime_error("Missing property table");
 for(std::int32_t i=0;i<count;++i){
  auto field=read<std::uintptr_t>(table,static_cast<std::size_t>(i)*8);if(!field)throw std::runtime_error("Missing property field");
  auto type=query(field);
  if(cached_type&&read<std::uintptr_t>(cached_type,72)){
   std::size_t steps=0;while(type&&type!=cached_type){if(++steps>4096)throw std::runtime_error("Missing bounded type hierarchy");type=read<std::uintptr_t>(type,64);}
  }
  if(type==cached_type&&read<std::uintptr_t>(field,80)==identity)callback(field);
 }
}
}
