#include "ctr/cold_name_pool_create.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation create_cold_name_pool(std::span<const std::byte> descriptor,
 std::span<const std::byte> manager,std::span<std::byte> pool,
 SystemPoolBackend& backend,bool poison,std::uint8_t global_selector,
 std::uint64_t& publication,std::span<std::byte> previous,std::uint64_t previous_token){
 if(descriptor.size()!=176||descriptor[0]!=std::byte{0}||manager.size()!=112||manager[0x6c]!=std::byte{0}||pool.size()!=176)
  throw std::invalid_argument("Cold descriptor/nonthreaded manager required");
 const auto pool_token=reinterpret_cast<std::uintptr_t>(pool.data());
 auto selected=[&](std::size_t offset){std::uint64_t token;std::memcpy(&token,manager.data()+offset,8);return token;};
 if(selected(global_selector?0x28:0x18)!=pool_token||selected((global_selector&1)?0x28:0x18)!=pool_token)
  throw std::invalid_argument("Creation and tag selectors must resolve actual supplied pool");
 if((publication&&(publication!=previous_token||previous_token!=0x10006000||previous.size()!=64))||(!publication&&(!previous.empty()||previous_token)))
  throw std::invalid_argument("Resolved previous name pool64 required");
 std::uint32_t type;std::memcpy(&type,pool.data()+0x1c,4);
 auto result=dispatch_null_reallocation_system_pool(pool,backend,64,8,poison);
 auto s=result.payload;const std::uint64_t vtable=0x025c4510,zero=0,initial=1024;
 const std::uint32_t refs=(type<<22)|1u;
 std::memcpy(s.data(),&vtable,8);std::memcpy(s.data()+8,&refs,4);
 std::memset(s.data()+0xc,0,4);std::memcpy(s.data()+0x10,&zero,8);std::memcpy(s.data()+0x18,&initial,8);
 std::memset(s.data()+0x20,0,8);s[0x28]=std::byte{0};
 if(publication){std::memcpy(s.data()+0x30,previous.data()+0x30,8);std::memcpy(previous.data()+0x30,&result.address,8);}
 publication=result.address;
 return result; // Original wrapper void; publication carries its actual pointer.
}
}
