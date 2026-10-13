#include "ctr/core/pointer_resolver.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint64_t resolve_registered_pointer(std::span<const std::byte> metadata,
 std::uint64_t key, std::int32_t count, std::span<const std::byte> records) {
 if(metadata.size()<93)throw std::invalid_argument("Metadata flag extent missing");
 if((std::to_integer<unsigned>(metadata[92])&32u)==0)return 0;
 if(count<0 || static_cast<std::size_t>(count)>records.size()/16)
  throw std::invalid_argument("Registered pointer table extent missing");
 std::uint64_t previous=0;
 for(std::int32_t i=0;i<count;++i) {
  std::uint64_t current;std::memcpy(&current,records.data()+static_cast<std::size_t>(i)*16,8);
  if(i && current<=previous)throw std::invalid_argument("Sorted unique pointer keys required");
  previous=current;
 }
 std::size_t first=0, remaining=static_cast<std::size_t>(count);
 while(remaining) {
  const auto half=remaining/2, middle=first+half;
  std::uint64_t current;std::memcpy(&current,records.data()+middle*16,8);
  if(current<key) {first=middle+1;remaining-=half+1;}
  else remaining=half;
 }
 if(first==static_cast<std::size_t>(count))return 0;
 std::uint64_t found,value;std::memcpy(&found,records.data()+first*16,8);
 if(found!=key)return 0;
 std::memcpy(&value,records.data()+first*16+8,8);return value;
}
}
