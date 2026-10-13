#include "ctr/cold_name_pool_activation.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void activate_created_name_pool(std::span<std::byte> object,
 std::span<const std::byte> manager,std::span<std::byte> pool,
 std::uint8_t global_selector,std::int32_t flag){
 if(object.size()!=64)throw std::invalid_argument("Created name context64 required");
 if(!flag){object[0x28]=std::byte{0};return;}
 if(manager.size()!=112||manager[0x6c]!=std::byte{0}||pool.size()!=176)
  throw std::invalid_argument("Actual nonthreaded manager/System Pool required");
 std::uint64_t identity;std::uint32_t refs,type;std::memcpy(&identity,object.data(),8);std::memcpy(&refs,object.data()+8,4);std::memcpy(&type,pool.data()+0x1c,4);
 if(identity!=0x025c4510||(refs>>22)!=1||type!=1)throw std::invalid_argument("Created System Pool1 name context required");
 auto selected=[&](std::size_t offset){std::uint64_t token;std::memcpy(&token,manager.data()+offset,8);return token;};
 const auto token=reinterpret_cast<std::uintptr_t>(pool.data());
 if(selected(global_selector==1?0x28:0x18)!=token||selected(0x10)!=token)
  throw std::invalid_argument("Both actual selectors must resolve supplied System Pool");
 // Original first writes flag lowbyte, then1 to the same actual selected pool.
 pool[0x38]=std::byte{static_cast<std::uint8_t>(flag)};pool[0x38]=std::byte{1};
 object[0x28]=std::byte{static_cast<std::uint8_t>(flag)};
}
}
