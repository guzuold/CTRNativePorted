// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/root_metadata_initializer.h"
#include "ctr/core/metadata_registry_publish_linear.h"
#include "ctr/core/metadata_registration_append.h"
#include "ctr/core/instance_release_nonterminal.h"
#include "ctr/metaobject_parent_zero_fields.h"
#include "ctr/metaobject_zero_field_finalize.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uintptr_t root_metadata_initializer(std::span<std::byte> m,std::uintptr_t base,
 std::size_t root_slot,std::size_t base_slot,const MetadataNameOriginal& intern,
 const std::function<std::uintptr_t()>& create,const std::function<std::uintptr_t()>& list,
 const PoolFreshOriginal& allocate){
 if(m.size()!=16384||base!=0x10001000||root_slot!=10496||base_slot!=10504)throw std::runtime_error("Missing root fixture adapter geometry");
 if(m[8192]!=std::byte{}||m[7200+0x6c]!=std::byte{}||m[11392+0x28]!=std::byte{})throw std::runtime_error("Missing raw unsynchronized root bootstrap domain");
 auto check=[&](std::size_t o,std::size_t n){if(o>m.size()||n>m.size()-o)throw std::runtime_error("Missing root metadata extent");};
 auto off=[&](std::uintptr_t p){if(p<base||p-base>=m.size())throw std::runtime_error("Missing resolved root pointer");return static_cast<std::size_t>(p-base);};
 auto q=[&](std::size_t o){check(o,8);std::uintptr_t v;std::memcpy(&v,m.data()+o,8);return v;};
 auto putq=[&](std::size_t o,std::uintptr_t v){check(o,8);std::memcpy(m.data()+o,&v,8);};
 auto flags=[&](std::size_t o,std::uint16_t v){check(o+0x5c,2);std::memcpy(m.data()+o+0x5c,&v,2);};
 if(q(root_slot)||q(base_slot)||!create||!list||!intern||!allocate)throw std::runtime_error("Missing cold root providers/domain");
 auto begin=[&](std::size_t slot){auto p=create();auto o=off(p);check(o,152);std::uint16_t initial_flags;std::int32_t fields,secondary;std::memcpy(&initial_flags,m.data()+o+0x5c,2);std::memcpy(&fields,m.data()+o+0x18,4);std::memcpy(&secondary,m.data()+o+0x68,4);if(initial_flags||fields||secondary||q(o+0x40)||q(o+0x10))throw std::runtime_error("Missing fresh root metadata");putq(slot,p);flags(o,8);return o;};
 auto finish=[&](std::size_t o,std::uintptr_t query){
  flags(o,9);
  metadata_name_associate(std::span<std::byte,24>(m.data()+o,24),base+11392,reinterpret_cast<const char*>(query),intern,{});
  std::uint16_t size=16;std::memcpy(m.data()+o+0x5a,&size,2);
  putq(o+0x38,0x4025ba3c0);flags(o,0x409);
  flags(o,0x40b);metadata_registry_publish_linear(m,base,o,8192,9216);
  instance_release_nonterminal(m.subspan(o,152));finalize_zero_field_metaobject_storage(m.subspan(o,152));
 };
 auto root=begin(root_slot);
 auto parent=begin(base_slot);
 auto result=list();finish(parent,base+11792);flags(parent,0x407);
 auto prior=q(parent+0x48);if(prior)check(off(prior),152);auto head=prior?m.subspan(off(prior),152):std::span<std::byte>{};
 link_metaobject_parent_zero_fields(m.subspan(root,152),m.subspan(parent,152),head,base+root,base+parent,prior);
 finish(root,base+13000);
 metadata_registration_append(m,base,off(result),0x401ab04f8,7200,1,allocate);
 flags(root,0x407);return result;
}
}
