// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/instance_initialize_fields.h"
#include "ctr/core/instance_copy_scalar_field.h"
#include <cstring>
#include <stdexcept>
namespace ctr { namespace {
template<class T>T read(const std::byte* p,std::size_t o){T v;std::memcpy(&v,p+o,sizeof(v));return v;}
std::uintptr_t method(std::uintptr_t obj,std::size_t slot){auto p=reinterpret_cast<const std::byte*>(obj);return read<std::uintptr_t>(reinterpret_cast<const std::byte*>(read<std::uintptr_t>(p,0)),slot);}
}
static void initialize_impl(std::span<std::byte> destination,std::span<const std::byte> source,
 const InstanceInitializationDefaults& defaults,const InstanceTypeOriginal& type_query,
 const InstanceFieldOriginal& field_dispatch,const InstancePostOriginal& post_dispatch,bool alternate){
 if(destination.size()<8||source.size()<8||!defaults.metadata||!defaults.type_query_target||!defaults.scalar_copy_target||!defaults.post_target)
  throw std::runtime_error("Missing instance defaults/layout");
 auto dst=reinterpret_cast<std::uintptr_t>(destination.data()),src=reinterpret_cast<std::uintptr_t>(source.data());
 auto query=[&](std::uintptr_t obj){
  if(method(obj,0)==defaults.type_query_target)return defaults.metadata;
  if(!type_query)throw std::runtime_error("Missing original instance type query");
  auto value=type_query(obj);if(!value)throw std::runtime_error("Missing instance type metadata");
  return reinterpret_cast<const std::byte*>(value);
 };
 auto d=query(dst);
 if(alternate){
  if(!post_dispatch)throw std::runtime_error("Missing original preparation virtuala8");
  post_dispatch(dst,0xa8u,0);post_dispatch(src,0xa8u,0);
 }
 auto table=reinterpret_cast<const std::byte*>(read<std::uintptr_t>(d,40));
 auto count=read<std::int32_t>(d,24);auto s=query(src);
 if(count>=read<std::int32_t>(s,24))count=read<std::int32_t>(query(src),24);
 auto first=defaults.base_field_count;
 if(first<0||count<first)throw std::runtime_error("Missing valid field range");
 if(count>first&&!table)throw std::runtime_error("Missing field table");
 for(auto i=first;i<count;++i){
  auto field=read<std::uintptr_t>(table,static_cast<std::size_t>(i)*8);if(!field)throw std::runtime_error("Missing field record");
  auto p=reinterpret_cast<const std::byte*>(field);auto flags=read<std::uint32_t>(p,32);
  auto mode=alternate?(flags&3u):(((~flags)&3u)?(flags&3u):1u);auto slot=mode==1?0x188u:mode==2?0x190u:0x180u;
  if(mode==1&&method(field,slot)==defaults.scalar_copy_target)
   instance_copy_scalar_field(std::span<const std::byte,24>(p,24),destination,source);
  else{if(!field_dispatch)throw std::runtime_error("Missing original field dispatch");field_dispatch(field,slot,dst,src);}
 }
 auto post_slot=alternate?0x30u:0x38u;
 if(alternate||method(dst,post_slot)!=defaults.post_target){
  if(!post_dispatch)throw std::runtime_error("Missing original post dispatch");post_dispatch(dst,post_slot,src);
 }
}
void instance_initialize_fields(std::span<std::byte> d,std::span<const std::byte> s,const InstanceInitializationDefaults& defaults,const InstanceTypeOriginal& q,const InstanceFieldOriginal& f,const InstancePostOriginal& p){initialize_impl(d,s,defaults,q,f,p,false);}
void instance_initialize_fields_alternate(std::span<std::byte> d,std::span<const std::byte> s,const InstanceInitializationDefaults& defaults,const InstanceTypeOriginal& q,const InstanceFieldOriginal& f,const InstancePostOriginal& p){initialize_impl(d,s,defaults,q,f,p,true);}

}

