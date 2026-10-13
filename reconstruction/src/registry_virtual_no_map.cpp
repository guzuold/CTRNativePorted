#include "ctr/registry_virtual_no_map.h"
#include "ctr/registry_append_no_map.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint32_t register_metadata_lookup_disabled(std::span<std::byte> manager,
 std::span<std::byte> header,std::uint64_t header_token,
 std::span<std::byte> table,std::uint64_t table_token,
 std::span<std::byte> metadata,std::uint64_t metadata_token){
 if(manager.size()!=128)throw std::invalid_argument("Manager128 required");
 if(manager[0]!=std::byte{0})throw std::invalid_argument("Enabled registry lookup missing");
 std::uint64_t mutex;std::memcpy(&mutex,manager.data()+0x70,8);
 if(mutex!=0)throw std::invalid_argument("Registry mutex resolution missing");
 // Original014e41a0 reads manager byte0 and returns null in this exact domain.
 append_registry_without_map(manager,header,header_token,table,table_token,metadata,metadata_token);
 std::int32_t count;std::memcpy(&count,header.data(),4);
 std::uint32_t index=0;
 for(;index<static_cast<std::uint32_t>(count);++index){
  std::uint64_t token;std::memcpy(&token,table.data()+index*8,8);
  if(token==metadata_token)break;
 }
 const auto stored=static_cast<std::uint16_t>(index);std::memcpy(metadata.data()+0x58,&stored,2);
 return 0;
}
}
