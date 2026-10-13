#include "ctr/cold_typed_fields.h"
#include "ctr/system_pool_mode1.h"
#include <array>
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation create_cold_typed_field(std::span<const std::byte> descriptor,
 std::span<const std::byte> manager,std::span<std::byte> pool,SystemPoolBackend& b,ColdFieldKind kind,bool poison){
 const auto index=static_cast<std::uint32_t>(kind);
 if(index>3 || descriptor.size()!=176 || manager.size()!=112 || descriptor[0]!=std::byte{0} || manager[0x6c]!=std::byte{0})
  throw std::invalid_argument("Known cold field type and nonthreaded manager required; warm factory missing");
 std::uint64_t selected;std::memcpy(&selected,manager.data()+0x10,8);
 if(selected!=reinterpret_cast<std::uintptr_t>(pool.data()))throw std::invalid_argument("Actual selected pool required");
 constexpr std::array<std::uint64_t,4> sizes{64,80,72,112},identities{0x025c3090,0x025bba10,0x025c2918,0x025c4220};
 auto result=dispatch_null_reallocation_system_pool(pool,b,sizes[index],8,poison);auto field=result.payload;
 const auto identity=identities[index];const std::uint64_t minus_one=0xffffffffull,zero64=0;
 const std::uint32_t refs=1,reference_flags=0x01000000,zero32=0,minus32=0xffffffffu;const std::uint16_t zero16=0;
 std::uint32_t flags;std::memcpy(&flags,field.data()+0x20,4);flags=(flags&0xfe000000u)|0x403ffu;
 std::memcpy(field.data(),&identity,8);std::memcpy(field.data()+8,&refs,4);
 std::memcpy(field.data()+0xc,&minus_one,8);std::memcpy(field.data()+0x14,&zero16,2);
 std::memcpy(field.data()+0x18,&zero64,8);std::memcpy(field.data()+0x20,&flags,4);
 std::memcpy(field.data()+0x28,&zero64,8);std::memset(field.data()+0x30,0,16);
 if(kind!=ColdFieldKind::UnsignedShort)std::memcpy(field.data()+0x40,&reference_flags,4);
 if(kind==ColdFieldKind::ObjectReference)std::memcpy(field.data()+0x48,&zero64,8);
 if(kind==ColdFieldKind::Vector){
  std::memset(field.data()+0x48,0,16);std::memcpy(field.data()+0x58,&minus32,4);
  std::memcpy(field.data()+0x60,&zero64,8);std::memcpy(field.data()+0x68,&zero32,4);
 }
 return result;
}
}
