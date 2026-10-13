#include "ctr/descriptor_manager.h"
#include "ctr/pool_manager.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
void mark_dispatch_descriptor_bootstrap_started(std::span<std::byte> d){
 if(d.size()!=176)throw std::invalid_argument("Descriptor176 required");d[0x14]=std::byte{1};
}
void construct_allocated_descriptor_manager(std::span<std::byte> d,std::span<std::byte> m,std::uint64_t& publication){
 if(d.size()!=176||m.size()!=112)throw std::invalid_argument("Actual descriptor176/manager112 required");
 const auto da=reinterpret_cast<std::uintptr_t>(d.data()),ma=reinterpret_cast<std::uintptr_t>(m.data()),pa=reinterpret_cast<std::uintptr_t>(&publication);
 if((da<ma+112&&ma<da+176)||(pa<da+176&&da<pa+8)||(pa<ma+112&&ma<pa+8))throw std::invalid_argument("Separate descriptor/manager/publication storage required");
 const std::uint64_t table=0x025babf0,zero=0,address=static_cast<std::uint64_t>(ma);
 std::memcpy(m.data(),&table,8);std::memcpy(m.data()+0x58,&zero,8);std::memset(m.data()+0x30,0,32);
 reset_unowned_pool_manager(m);
 std::memcpy(d.data()+0x78,&address,8);
 // Original table+40=014ec870 publishes the manager pointer; external native cell adapter.
 publication=address;
}
}
