#include "ctr/core/object_type_query.h"
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {template<class T>T load(std::span<const std::byte>b,std::size_t at){T x;std::memcpy(&x,b.data()+at,sizeof(x));return x;}}
bool query_prepared_object_type(std::span<const std::byte> fixture){
 if(fixture.size()!=672)throw std::invalid_argument("Explicit type query bindings required");
 const auto expected=load<std::uint64_t>(fixture,0),object=load<std::uint64_t>(fixture,8);
 if(!object)return true;
 if(object!=0x10001000)throw std::invalid_argument("Unbound object");
 const auto getter=load<std::uint64_t>(fixture,16);
 // Actual ObjectRef/Vector/igObject/metaobject +0 providers read their cache then RET.
 if(getter!=0x0131c2a0 && getter!=0x013a4ef0 && getter!=0x013e02c0 &&
    getter!=0x01451580 && getter!=0x003786c0 && getter!=0x013e3e00)
  throw std::invalid_argument("Other virtual type getter missing");
 auto node=[&](std::uint64_t token){
  if(token<0x10003000 || (token-0x10003000)%128 || (token-0x10003000)/128>=8)
   throw std::invalid_argument("Unbound metadata");
  return fixture.subspan(32+static_cast<std::size_t>((token-0x10003000)/128)*80,80);
 };
 auto actual=load<std::uint64_t>(fixture,24);
 if(expected && load<std::uint64_t>(node(expected),72)){
  unsigned visited=0;
  while(actual && actual!=expected){
   if(++visited>8)throw std::invalid_argument("Cyclic or unavailable metadata hierarchy");
   actual=load<std::uint64_t>(node(actual),64);
  }
 }
 return actual==expected;
}
}
