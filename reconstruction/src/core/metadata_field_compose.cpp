// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_field_compose.h"
#include "ctr/core/field_alignment.h"
#include "ctr/core/metadata_identifier_lookup.h"
#include "ctr/metaobject_field_append.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {
template<class T> T load(const std::byte*p){T v{};std::memcpy(&v,p,sizeof(v));return v;}
template<class T> void store(std::byte*p,T v){std::memcpy(p,&v,sizeof(v));}
}
void metadata_field_compose_default8(std::span<std::byte,152> metadata,
 std::span<std::byte,64> table,std::span<std::byte,80> field,
 std::uintptr_t field_type,std::uintptr_t special_type,
 const std::byte* value_type,const std::byte* registry){
 auto*m=metadata.data();auto*f=field.data();
 auto count=load<std::int32_t>(m+0x18);
 if(count<0||count>=8)throw std::runtime_error("Missing field backing/growth domain");
 std::uint32_t offset=0;
 if(field_type==special_type){
  auto inherited=load<std::uintptr_t>(f+0x48);
  if(inherited)offset=load<std::uint16_t>(reinterpret_cast<const std::byte*>(inherited)+0x14);
 }else{
  std::uint32_t end=11;const std::byte* last=nullptr;
  for(std::int32_t i=0;i<count;++i){
   auto token=load<std::uintptr_t>(table.data()+i*8);
   if(!token)throw std::runtime_error("Missing valid existing descriptor");
   auto* candidate=reinterpret_cast<const std::byte*>(token);
   if(!last||load<std::uint16_t>(candidate+0x14)>load<std::uint16_t>(last+0x14))last=candidate;
  }
  if(last)end=std::uint32_t(load<std::uint16_t>(last+0x14))+load<std::uint16_t>(last+0x12)-1;
  auto alignment=field_alignment_default8(std::span<std::byte,36>(f,36));
  auto old=load<std::uint16_t>(m+0x5e);auto low=static_cast<std::uint16_t>(alignment);
  store(m+0x5e,low<old?old:low);offset=(0u-alignment)&(end+alignment);
 }
 store(f+0x14,static_cast<std::uint16_t>(offset));
 auto id=load<std::uint16_t>(f+0xc);bool assign=id==0xffff;
 if(!assign){
  if(!registry)throw std::runtime_error("Missing identifier registry26edbb8");
  assign=metadata_identifier_lookup(std::span<const std::byte,120>(registry,120),id)==0;
 }
 if(assign)store(f+0xc,load<std::uint16_t>(m+0x58));
 store(f+0x10,static_cast<std::uint16_t>(count));
 auto end=offset+8u;auto size=load<std::uint16_t>(m+0x5a);
 if(static_cast<std::int32_t>(end)>=size)size=static_cast<std::uint16_t>(end);
 store(m+0x5a,size);
 store(f+0xe,value_type?load<std::uint16_t>(value_type+0x10):std::uint16_t{0xffff});
 store(f+0x12,std::uint16_t{8});
 append_metaobject_field_no_growth(metadata,table,reinterpret_cast<std::uintptr_t>(table.data()),
  field.first<32>(),reinterpret_cast<std::uintptr_t>(f));
 if((m[0x5c]&std::byte{2})!=std::byte{0}&&load<std::uintptr_t>(m+0x48))
  throw std::runtime_error("Missing inherited clone13db760/pool services and recursive compose");
}
}
