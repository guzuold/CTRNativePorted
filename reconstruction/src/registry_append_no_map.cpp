#include "ctr/registry_append_no_map.h"
#include "ctr/registry_array_append.h"
#include <array>
#include <cstring>
#include <stdexcept>
namespace ctr {
void append_registry_without_map(std::span<std::byte> manager,
 std::span<std::byte> header,std::uint64_t header_token,
 std::span<std::byte> table,std::uint64_t table_token,
 std::span<std::byte> metadata,std::uint64_t metadata_token){
 if(manager.size()!=128||metadata.size()!=152||metadata_token==0)
  throw std::invalid_argument("Resolved manager128/metadata152 required");
 std::uint64_t actual_header,map;std::memcpy(&actual_header,manager.data()+0x20,8);std::memcpy(&map,manager.data()+0x30,8);
 if(map!=0)throw std::invalid_argument("Registry map insertion missing");
 if(actual_header==0||actual_header!=header_token)throw std::invalid_argument("Registry array resolution missing");
 std::array<std::byte,8> source;std::memcpy(source.data(),&metadata_token,8);
 // Disjoint resolved spans, single-thread final-storage contract. Validate and
 // insert first so unsupported growth leaves all supplied storage unchanged.
 append_registry_array_storage(header,table,table_token,source);
 std::uint32_t flags,mode;std::memcpy(&flags,metadata.data()+8,4);++flags;std::memcpy(metadata.data()+8,&flags,4);
 std::memcpy(&mode,manager.data()+0x18,4);
 if(mode==0){std::uint32_t counter;std::memcpy(&counter,manager.data()+0x64,4);++counter;std::memcpy(manager.data()+0x64,&counter,4);}
}
}
