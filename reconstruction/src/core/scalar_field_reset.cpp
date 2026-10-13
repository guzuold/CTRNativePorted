#include "ctr/core/scalar_field_reset.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {
template<class T> T load(std::span<const std::byte> bytes,std::size_t at) {
 T value; std::memcpy(&value,bytes.data()+at,sizeof(value)); return value;
}
}
void reset_prepared_scalar_field(std::span<const std::byte> field,
 std::span<std::byte> object,std::span<const std::byte> backing,std::uint64_t token) {
 if(field.size()!=72 || object.size()!=128 || backing.size()!=16)
  throw std::invalid_argument("Explicit prepared field/object/default bindings required");
 const auto table=load<std::uint64_t>(field,0);
 if(table!=0x025bb470 && table!=0x025c3090 && table!=0x025c2918)
  throw std::invalid_argument("Object/vector or unknown reset providers missing");
 const auto mode=(std::to_integer<unsigned>(field[32])>>2)&3u;
 if(mode==2) return; // Actual +148 target014de6d0 is RET.
 if(mode==1) throw std::invalid_argument("Reset-by-reference libc dispatch missing");
 const auto width=load<std::uint16_t>(field,18);
 const auto offset=load<std::uint16_t>(field,20);
 if(width!=1 && width!=2 && width!=4 && width!=8)
  throw std::invalid_argument("Generic width libc fallback missing");
 if(offset>object.size() || width>object.size()-offset)
  throw std::invalid_argument("Field lies outside bound object");
 const bool inline_value=(load<std::uint64_t>(field,48)&(std::uint64_t{1}<<56)) && width<=4;
 const auto pointer=load<std::uint64_t>(field,56);
 if(inline_value) {
  std::memcpy(object.data()+offset,field.data()+56,width);
 } else if(pointer) {
  if(pointer!=token || !token) throw std::invalid_argument("External default binding missing");
  if(width==8) throw std::invalid_argument("Eight-byte default memcpy dispatch missing");
  std::memcpy(object.data()+offset,backing.data(),width);
 } else {
  if(width==2) throw std::invalid_argument("Two-byte zero memset dispatch missing");
  std::memset(object.data()+offset,0,width);
 }
}
}
