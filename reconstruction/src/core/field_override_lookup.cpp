#include "ctr/core/field_override_lookup.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint64_t lookup_field_override(std::span<const std::byte> field,
 std::uint64_t field_token,std::int32_t count,std::span<const std::byte> pairs) {
 if(field.size()<35) throw std::invalid_argument("Field flags unavailable");
 if((std::to_integer<unsigned>(field[34])&16u)==0) return 0;
 if(count==0) return 0;
 if(count<0 || static_cast<std::size_t>(count)>pairs.size()/16)
  throw std::invalid_argument("Original override table extent unavailable");
 for(std::int32_t i=0;i<count;++i) {
  std::uint64_t key,value;
  const auto at=static_cast<std::size_t>(i)*16;
  std::memcpy(&key,pairs.data()+at,8);
  if(key!=field_token) continue;
  std::memcpy(&value,pairs.data()+at+8,8);
  return value;
 }
 return 0;
}
}
