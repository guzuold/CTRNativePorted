#include "ctr/cold_scalar_field64.h"
#include "ctr/system_pool_mode1.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation create_cold_scalar_field64(std::span<const std::byte> descriptor,
 std::span<const std::byte> manager,std::span<std::byte> pool,SystemPoolBackend& b,bool poison){
 if(descriptor.size()!=176 || manager.size()!=112 || descriptor[0]!=std::byte{0} || manager[0x6c]!=std::byte{0})
  throw std::invalid_argument("Cold descriptor and nonthreaded manager required; warm field factory missing");
 std::uint64_t selected;std::memcpy(&selected,manager.data()+0x10,8);
 if(selected!=reinterpret_cast<std::uintptr_t>(pool.data()))throw std::invalid_argument("Actual selected pool required");
 auto result=dispatch_null_reallocation_system_pool(pool,b,64,8,poison);auto field=result.payload;
 const std::uint64_t identity=0x025bb470,minus_one=0xffffffffull,zero64=0;
 const std::uint32_t refs=1;const std::uint16_t zero16=0;
 std::uint32_t flags;std::memcpy(&flags,field.data()+0x20,4);flags=(flags&0xfe000000u)|0x403ffu;
 // Full0153c6b0 constructor, then concrete vtable write0154c45f..466.
 std::memcpy(field.data(),&identity,8);std::memcpy(field.data()+8,&refs,4);
 std::memcpy(field.data()+0xc,&minus_one,8);std::memcpy(field.data()+0x14,&zero16,2);
 std::memcpy(field.data()+0x18,&zero64,8);std::memcpy(field.data()+0x20,&flags,4);
 std::memcpy(field.data()+0x28,&zero64,8);std::memset(field.data()+0x30,0,16);
 return result;
}
}
