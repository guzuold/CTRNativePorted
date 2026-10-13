#include "ctr/growing_field_append.h"
#include <bit>
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation append_growing_metadata_field(std::span<std::byte> metadata,
 std::span<const std::byte> manager,std::span<std::byte> pool,SystemPoolBackend& b,
 const SystemPoolAllocation& table,std::span<std::byte> field,std::uint64_t token,bool poison){
 if(metadata.size()!=152 || (field.size()!=0 && field.size()!=32) || (token==0)!=field.empty())
  throw std::invalid_argument("Metadata152/resolved field32 or null required");
 if(token){std::uint32_t refs;std::memcpy(&refs,field.data()+8,4);if(refs<1 || refs>0x7fffc)throw std::invalid_argument("Weak/lifetime services remain missing");}
 std::uint32_t count;std::memcpy(&count,metadata.data()+0x18,4);
 if(count!=8 && count!=12 && count!=16 && count!=24 && count!=32)
  throw std::invalid_argument("Only observed full arrays8/12/16/24/32 supported for growth append");
 // 014f4d80..014f4dee: requested count+1 >=8 and <=128 rounds upward.
 const auto capacity=std::bit_ceil(count+1);
 auto result=reserve_populated_field_array(metadata.subspan(0x18,24),manager,pool,b,table,capacity,poison);
 append_reserved_metadata_field(metadata,result,field,token);
 return result;
}
}
