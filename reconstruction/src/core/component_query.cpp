#include "ctr/core/component_query.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint64_t query_registered_component(std::span<const std::byte> image,
 std::uint64_t base, std::uint64_t object, std::uint64_t expected,
 std::uint32_t ordinal, std::uint64_t igobject_type) {
 auto read=[&]<class T>(std::uint64_t address) {
  if(address<base || address-base>image.size() || sizeof(T)>image.size()-(address-base))
   throw std::invalid_argument("Prepared component address extent missing");
  T value;std::memcpy(&value,image.data()+static_cast<std::size_t>(address-base),sizeof(T));return value;
 };
 if(!expected)return 0;
 const auto table=read.operator()<std::uint64_t>(object+24);
 const auto data=read.operator()<std::uint64_t>(table+32);
 const auto count=read.operator()<std::int32_t>(table+12);
 const bool derived=read.operator()<std::uint64_t>(expected+72)!=0;
 if(count<0)throw std::invalid_argument("Negative component count unsupported");
 std::uint32_t matches=0;
 for(std::int32_t i=0;i<count;++i) {
  const auto component=read.operator()<std::uint64_t>(data+static_cast<std::uint64_t>(i)*8);
  if(!component || (read.operator()<std::uint8_t>(component+32)&16u))continue;
  const auto vtable=read.operator()<std::uint64_t>(component);
  if(read.operator()<std::uint64_t>(vtable)!=0x3786c0u)
   throw std::runtime_error("missing: original component virtual metadata getter");
  auto actual=igobject_type; // Whole original getter003786c0 reads cache026ee130.
  bool match=false;
  if(derived) {
   std::size_t steps=0;
   while(actual && actual!=expected) {
    if(++steps>image.size()/8)throw std::invalid_argument("Metadata hierarchy cycle");
    actual=read.operator()<std::uint64_t>(actual+64);
   }
   match=actual==expected;
  } else match=actual==expected;
  if(match) {if(matches==ordinal)return component;++matches;}
 }
 return 0;
}
}
