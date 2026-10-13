// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_finalize_layout.h"
#include "ctr/core/metadata_identifier_lookup.h"
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace ctr {
void metadata_finalize_layout(std::span<std::byte,128> metadata,const std::byte* identifier_registry,const MetadataOriginalVirtual* original_virtual){
 std::int32_t fields{},count{};std::uint16_t alignment{},other{},identity{};std::uintptr_t backing{};
 std::memcpy(&fields,metadata.data()+0x18,4);
 std::int32_t maximum=4;
 if(fields>0){
  if(!original_virtual)throw std::runtime_error("Missing original field virtual provider+130/+260");
  std::uintptr_t table{},parent{};std::memcpy(&table,metadata.data()+0x28,8);std::memcpy(&parent,metadata.data()+0x40,8);
  if(!table)throw std::runtime_error("Missing field backing");
  auto query=[&](std::uint16_t id){if(!identifier_registry)throw std::runtime_error("Missing identifier registry26edbb8");return metadata_identifier_lookup(std::span<const std::byte,120>(identifier_registry,120),id);};
  for(std::int32_t i=0;i<fields;++i){
   std::uintptr_t token{};std::memcpy(&token,reinterpret_cast<const std::byte*>(table)+static_cast<std::size_t>(i)*8,8);
   if(!token)throw std::runtime_error("Missing field descriptor");
   auto*field=reinterpret_cast<std::byte*>(token);
   auto value=static_cast<std::int32_t>((*original_virtual)(token,0x130,0));if(value>=maximum)maximum=value;
   std::uint16_t id{};std::memcpy(&id,field+0xc,2);bool index=true;
   if(parent){std::int32_t inherited{};std::memcpy(&inherited,reinterpret_cast<const std::byte*>(parent)+0x18,4);if(i<inherited&&id!=0xffff&&query(id))index=false;}
   if(index){auto low=static_cast<std::uint16_t>(i);std::memcpy(field+0x10,&low,2);}
   if(id==0xffff||!query(id))(*original_virtual)(token,0x260,reinterpret_cast<std::uintptr_t>(metadata.data()));
  }
 }
 std::memcpy(&alignment,metadata.data()+0x5e,2);auto low=static_cast<std::uint16_t>(maximum);if(alignment<low)alignment=low;std::memcpy(metadata.data()+0x5e,&alignment,2);
 std::memcpy(&other,metadata.data()+0x62,2);if(other<4)other=4;std::memcpy(metadata.data()+0x62,&other,2);
 std::memcpy(&count,metadata.data()+0x68,4);std::memcpy(&backing,metadata.data()+0x78,8);std::memcpy(&identity,metadata.data()+0x58,2);
 if(count>0){if(!backing)throw std::runtime_error("Missing descriptor backing");
  for(std::int32_t i=0;i<count;++i){std::uintptr_t descriptor{};std::memcpy(&descriptor,reinterpret_cast<const std::byte*>(backing)+static_cast<std::size_t>(i)*8,8);if(!descriptor)throw std::runtime_error("Missing descriptor");
   auto*slot=reinterpret_cast<std::byte*>(descriptor)+0x38;std::uint16_t identifier{};std::memcpy(&identifier,slot,2);
   if(identifier!=0xffff){
    if(!identifier_registry)throw std::runtime_error("Missing identifier registry26edbb8");
    if(metadata_identifier_lookup(std::span<const std::byte,120>(identifier_registry,120),identifier))continue;
   }
   std::memcpy(slot,&identity,2);
  }
 }
 metadata[0x5c]|=std::byte{4};
}
}
