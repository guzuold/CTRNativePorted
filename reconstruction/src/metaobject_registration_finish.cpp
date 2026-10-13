#include "ctr/metaobject_registration_finish.h"
#include "ctr/registry_virtual_no_map.h"
#include "ctr/object_nonterminal_release.h"
#include "ctr/metaobject_zero_field_finalize.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint64_t finish_prepared_metaobject_registration(std::span<std::byte> manager,
 std::span<std::byte> header,std::uint64_t header_token,
 std::span<std::byte> table,std::uint64_t table_token,
 std::span<std::byte> metadata,std::uint64_t metadata_token,std::uint64_t base_token){
 if(metadata.size()!=152)throw std::invalid_argument("Metadata152 required");
 std::uint64_t parent;std::uint32_t refs;std::int32_t fields,secondary;std::uint16_t flags;
 std::memcpy(&parent,metadata.data()+0x40,8);std::memcpy(&refs,metadata.data()+8,4);
 std::memcpy(&fields,metadata.data()+0x18,4);std::memcpy(&secondary,metadata.data()+0x68,4);std::memcpy(&flags,metadata.data()+0x5c,2);
 if(parent||refs!=1||fields!=0||secondary!=0||flags!=0x419)
  throw std::invalid_argument("Prepared419/null-parent/no-fields/ref1 continuation required");
 register_metadata_lookup_disabled(manager,header,header_token,table,table_token,metadata,metadata_token);
 // Single-thread final storage: flag2 is stored after checked registration so
 // unsupported registry domains reject before mutation. Original stores it first.
 metadata[0x5c]|=std::byte{2};
 release_nonterminal_object_storage(metadata);
 finalize_zero_field_metaobject_storage(metadata);
 std::memcpy(&flags,metadata.data()+0x5c,2);flags=static_cast<std::uint16_t>((flags|4)&0xfff7);
 std::memcpy(metadata.data()+0x5c,&flags,2);
 return base_token;
}
}
