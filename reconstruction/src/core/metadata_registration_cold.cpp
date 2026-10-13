// SPDX-License-Identifier: GPL-2.0-only
#include "ctr/core/metadata_registration_cold.h"
#include "ctr/core/metadata_registry_publish_linear.h"
#include "ctr/core/instance_release_nonterminal.h"
#include "ctr/metaobject_zero_field_finalize.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint64_t metadata_registration_cold(std::span<std::byte> m,std::uintptr_t base,std::size_t slot,std::uintptr_t name_context,std::uint16_t size,const MetadataNameOriginal& intern,const char* name,const std::function<std::uintptr_t()>& callback,const std::function<std::uintptr_t()>& create){
 if(slot>m.size()||8>m.size()-slot||!create)throw std::runtime_error("Missing cold publication slot/provider");std::uintptr_t prior;std::memcpy(&prior,m.data()+slot,8);if(prior)throw std::runtime_error("Missing cold NULLslot domain");auto created=create();if(created<base||created-base>m.size())throw std::runtime_error("Missing allocated metadata");std::memcpy(m.data()+slot,&created,8);auto metadata=static_cast<std::size_t>(created-base);
 if(metadata>m.size()||152>m.size()-metadata)throw std::runtime_error("Missing bounded stage1 metadata");
 auto meta=m.subspan(metadata,152);std::uint16_t flags;std::memcpy(&flags,meta.data()+0x5c,2);
 std::int32_t fields,secondary;std::uintptr_t parent,previous;std::memcpy(&fields,meta.data()+0x18,4);std::memcpy(&secondary,meta.data()+0x68,4);std::memcpy(&parent,meta.data()+0x40,8);std::memcpy(&previous,meta.data()+0x10,8);
 if((flags&15)!=0||fields||secondary||parent||previous)throw std::runtime_error("Missing stage0 parent/fields/name release domain");
 flags|=8;std::memcpy(meta.data()+0x5c,&flags,2);
 if(!callback)throw std::runtime_error("Missing original stage0 callback");auto list=callback();flags|=1;std::memcpy(meta.data()+0x5c,&flags,2);
 metadata_name_associate(std::span<std::byte,24>(meta.data(),24),name_context,name,intern,{});
 std::memcpy(meta.data()+0x5a,&size,2);flags|=0x410;std::memcpy(meta.data()+0x5c,&flags,2);
 flags|=2;std::memcpy(meta.data()+0x5c,&flags,2);
 metadata_registry_publish_linear(m,base,metadata,8192,9216);
 instance_release_nonterminal(meta);
 finalize_zero_field_metaobject_storage(meta);
 std::memcpy(&flags,meta.data()+0x5c,2);flags=static_cast<std::uint16_t>((flags|4)&~8);std::memcpy(meta.data()+0x5c,&flags,2);return list;
}
}
