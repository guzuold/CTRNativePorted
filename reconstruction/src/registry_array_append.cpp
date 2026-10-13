#include "ctr/registry_array_append.h"
#include <cstring>
#include <limits>
#include <stdexcept>
namespace ctr {
void append_registry_array_storage(std::span<std::byte> header,std::span<std::byte> table,
 std::uint64_t table_token,std::span<const std::byte> source){
 if(header.size()!=24||source.size()%8!=0)throw std::invalid_argument("Array24/aligned pointer source required");
 std::int32_t count;std::uint64_t bytes,token;
 std::memcpy(&count,header.data(),4);std::memcpy(&bytes,header.data()+8,8);std::memcpy(&token,header.data()+16,8);
 const auto tag=bytes&0xffffffff00000000ull;
 if(tag!=0&&tag!=0x0010000000000000ull)
  throw std::invalid_argument("Unverified array capacity tag");
 bytes&=0xffffffffull; // Verified raw bytes or exact bootstrap tag at bit52.
 if(token==0||token!=table_token||bytes>table.size()||bytes%8!=0||count<0)
  throw std::invalid_argument("Resolved array backing missing/invalid");
 const auto n=source.size()/8;const auto old=static_cast<std::size_t>(count);
 if(old>bytes/8||n>bytes/8-old||n>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())-old)
  throw std::invalid_argument("Array reserve/growth missing");
 // Original scalar overlapping branch copies forward, not memmove semantics.
 for(std::size_t i=0;i<n;++i){std::uint64_t value;std::memcpy(&value,source.data()+i*8,8);std::memcpy(table.data()+(old+i)*8,&value,8);}
 count=static_cast<std::int32_t>(old+n);std::memcpy(header.data(),&count,4);
}
}
