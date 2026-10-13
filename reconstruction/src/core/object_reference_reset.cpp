#include "ctr/core/object_reference_reset.h"
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {template<class T>T load(std::span<const std::byte>b,std::size_t at){T x;std::memcpy(&x,b.data()+at,sizeof(x));return x;}}
void reset_prepared_object_reference(std::span<std::byte> fixture){
 if(fixture.size()!=224)throw std::invalid_argument("Explicit ObjectRef bindings required");
 const auto field=fixture.first(80);auto object=fixture.subspan(80,128);
 if(load<std::uint64_t>(field,0)!=0x025bba10)throw std::invalid_argument("Unknown ObjectRef provider");
 const auto offset=load<std::uint16_t>(field,20);
 if(offset>120)throw std::invalid_argument("Reference outside object");
 if(load<std::uint64_t>(field,72)!=0)throw std::invalid_argument("Type/hierarchy virtual queries missing");
 const bool inline_default=(load<std::uint64_t>(field,48)&(1ull<<56))!=0 && load<std::uint16_t>(field,18)<=4;
 std::uint64_t value=0;
 const auto pointer=load<std::uint64_t>(field,56);
 if(inline_default)value=pointer;
 else if(pointer){
  if(pointer!=0x10003000)throw std::invalid_argument("Default backing unavailable");
  value=load<std::uint64_t>(fixture,208);
 }
 if(field[65]!=std::byte{0} || field[67]!=std::byte{0}){
  if(value || load<std::uint64_t>(object,offset))throw std::invalid_argument("Reference ownership/release/reset services missing");
  return; // Original null-default owning path returns with old null unchanged.
 }
 if(!value && field[66]!=std::byte{0} && field[64]!=std::byte{0})
  throw std::invalid_argument("ObjectRef constructor factory dependency missing");
 std::memcpy(object.data()+offset,&value,8);
 // Null value with flag66 tailcalls014dd4c0; flag64==0 is original early RET.
}
}
