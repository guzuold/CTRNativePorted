// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_registration_stage1.h"
#include "ctr/core/metadata_registry_publish_linear.h"
#include "ctr/core/instance_release_nonterminal.h"
#include "ctr/metaobject_zero_field_finalize.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint64_t metadata_registration_stage1(std::span<std::byte> m,std::uintptr_t base,std::size_t metadata,std::uintptr_t name_context,std::uint16_t size,const MetadataNameOriginal& intern,const char* name){
 if(metadata>m.size()||152>m.size()-metadata)throw std::runtime_error("Missing bounded stage1 metadata");
 auto meta=m.subspan(metadata,152);std::uint16_t flags;std::memcpy(&flags,meta.data()+0x5c,2);
 std::int32_t fields,secondary;std::uintptr_t parent,previous;std::memcpy(&fields,meta.data()+0x18,4);std::memcpy(&secondary,meta.data()+0x68,4);std::memcpy(&parent,meta.data()+0x40,8);std::memcpy(&previous,meta.data()+0x10,8);
 if((flags&15)!=1||fields||secondary||parent||previous)throw std::runtime_error("Missing stage1 parent/fields/name release domain");
 flags|=8;std::memcpy(meta.data()+0x5c,&flags,2);
 metadata_name_associate(std::span<std::byte,24>(meta.data(),24),name_context,name,intern,{});
 std::memcpy(meta.data()+0x5a,&size,2);flags|=0x410;std::memcpy(meta.data()+0x5c,&flags,2);
 flags|=2;std::memcpy(meta.data()+0x5c,&flags,2);
 metadata_registry_publish_linear(m,base,metadata);
 instance_release_nonterminal(meta);
 finalize_zero_field_metaobject_storage(meta);
 std::memcpy(&flags,meta.data()+0x5c,2);flags=static_cast<std::uint16_t>((flags|4)&~8);std::memcpy(meta.data()+0x5c,&flags,2);return 0;
}
}
