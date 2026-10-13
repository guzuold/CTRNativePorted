#include "ctr/reserved_field_append.h"
#include "ctr/object_nonterminal_release.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation reserve_metadata_field_array(std::span<std::byte> metadata,
 std::span<const std::byte> manager,std::span<std::byte> pool,
 SystemPoolBackend& backend,std::uint32_t fields,bool poison){
 if(metadata.size()!=152)throw std::invalid_argument("Metadata152 required");
 return reserve_empty_field_array(metadata.subspan(0x18,24),manager,pool,backend,fields,poison);
}
void append_reserved_metadata_field(std::span<std::byte> metadata,
 const SystemPoolAllocation& table,std::span<std::byte> field,std::uint64_t field_token){
 if(metadata.size()!=152||table.payload.empty()||table.payload.size()>512||table.payload.size()%8||(!field.empty()&&field.size()!=32)||((field_token==0)!=field.empty()))
  throw std::invalid_argument("Actual reserved pointer array/resolved field32 required");
 std::int32_t count;std::uint64_t capacity,token;std::uint32_t refs=0;
 std::memcpy(&count,metadata.data()+0x18,4);std::memcpy(&capacity,metadata.data()+0x20,8);std::memcpy(&token,metadata.data()+0x28,8);
 if(count<0||static_cast<std::size_t>(count)>=table.payload.size()/8||token!=table.address||!token||capacity!=(0x0010000000000000ull|table.payload.size()))
  throw std::invalid_argument("Reserved backing/capacity mismatch or growth missing");
 if(field_token){
  std::memcpy(&refs,field.data()+8,4);if(refs<1||refs>0x7fffc)throw std::invalid_argument("Weak/lifetime services missing");
  ++refs;std::memcpy(field.data()+8,&refs,4);
 }
 std::memcpy(table.payload.data()+static_cast<std::size_t>(count)*8,&field_token,8);
 if(field_token){++refs;std::memcpy(field.data()+8,&refs,4);}
 ++count;std::memcpy(metadata.data()+0x18,&count,4);
 if(field_token)release_nonterminal_object_storage(field);
}
}
