#include "ctr/core/vector_field_default.h"
#include "ctr/core/field_override_lookup.h"
#include "ctr/memory_ref_default.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
namespace {template<class T>T load(std::span<const std::byte>b,std::size_t at){T x;std::memcpy(&x,b.data()+at,sizeof(x));return x;}}
void initialize_prepared_vector_default(std::span<std::byte> fixture) {
 if(fixture.size()!=808)throw std::invalid_argument("Explicit Vector bindings required");
 const auto field=fixture.first(112);auto object=fixture.subspan(112,128);
 const auto manager=fixture.subspan(240,112);const auto pools=fixture.subspan(352,160);
 const auto overrides=fixture.subspan(512,40);const auto pairs=fixture.subspan(552,256);
 if(load<std::uint64_t>(field,0)!=0x025c4220)throw std::invalid_argument("Unknown Vector provider");
 if(lookup_field_override(field,0x10001000,load<std::int32_t>(overrides,12),pairs)!=0)
  throw std::invalid_argument("Named pool override resolution missing");
 if(manager[108]!=std::byte{0})throw std::invalid_argument("TLS/handle manager queries missing");
 const auto offset=load<std::uint16_t>(field,20);
 if(offset>104)throw std::invalid_argument("Vector outside object");
 if(load<std::uint64_t>(object,offset+16)!=0 || (load<std::uint64_t>(object,offset+8)&0x000fffffffffffffull)!=0)
  throw std::invalid_argument("Existing container pool migration/free missing");
 const auto alignment=load<std::int32_t>(field,88);
 if(alignment==-1)throw std::invalid_argument("Virtual element alignment query missing");
 std::array<std::optional<std::int32_t>,4> indices;
 const std::size_t slots[4]={16,24,32,40};
 for(std::size_t i=0;i<4;++i) {
  const auto token=load<std::uint64_t>(manager,slots[i]);
  if(token==0)continue;
  if(token!=0x10004000+i*256)throw std::invalid_argument("Unbound manager pool");
  indices[i]=load<std::int32_t>(pools,i*40+28);
 }
 // Original0153d4c0 sees null pointer and zero token: installs selected pool index.
 // Original01551b30 applies the same alignment encoding as014dc540, at header+8.
 initialize_memory_ref_default(object,static_cast<std::uint16_t>(offset+8),alignment,indices);
}
}
