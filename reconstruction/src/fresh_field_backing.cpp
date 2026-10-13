#include "ctr/fresh_field_backing.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation initialize_fresh_field_backing(std::span<std::byte> field,
 std::span<const std::byte> manager,std::span<std::byte> pool,SystemPoolBackend& b,bool poison){
 if(field.size()<64||manager.size()!=112||pool.size()!=176||manager[108]!=std::byte{0})
  throw std::invalid_argument("Prepared nonthreaded fresh default contract required");
 std::uint64_t table,header,pointer,selected;std::uint32_t refs,flags;
 std::memcpy(&table,field.data(),8);std::memcpy(&header,field.data()+48,8);
 std::memcpy(&pointer,field.data()+56,8);std::memcpy(&selected,manager.data()+24,8);
 std::memcpy(&refs,field.data()+8,4);std::memcpy(&flags,field.data()+32,4);
 if(header||pointer||(refs>>22)||selected!=reinterpret_cast<std::uintptr_t>(pool.data()))
  throw std::invalid_argument("Fresh memory reference and actual slot0 binding required");
 std::uint16_t width;
 if(table==0x025bba10&&field.size()==80)width=8;
 else if(table==0x025c2918&&field.size()==72)width=8;
 else if(table==0x025c4220&&field.size()==112)width=24;
 else throw std::invalid_argument("Uncovered default type/extent");
 // +130 getters cache alignment in bits10..17, not a constant-return getter.
 if(!(flags&0x3fc00u))flags=(flags&0xffbc03ffu)|0x402000u;
 const auto alignment=(flags>>10)&255u;
 if(alignment!=8)throw std::invalid_argument("Only verified default alignment8 supported");
 //014e3c70 ->014dabb0 mode1/null; positive successful actual allocation only.
 auto result=dispatch_null_reallocation_system_pool(pool,b,width,alignment,poison);
 std::memset(result.payload.data(),0,width); // original8 direct store;24 original libc memset
 header=(std::uint64_t(1)<<52)|width;pointer=result.address;
 std::memcpy(field.data()+18,&width,2);std::memcpy(field.data()+32,&flags,4);
 std::memcpy(field.data()+48,&header,8);std::memcpy(field.data()+56,&pointer,8);
 return result;
}
}
