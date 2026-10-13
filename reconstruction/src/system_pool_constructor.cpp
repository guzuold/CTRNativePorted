#include "ctr/system_pool_backend.h"
#include "ctr/pool_manager.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
std::uint32_t construct_allocated_system_pool(std::span<std::byte> pool,
 std::span<std::byte> manager,SystemPoolBackend& backend){
 if(pool.size()!=0xb0 || manager.size()<0x70 || manager[0x6c]!=std::byte{0} || !backend.heap.header_token)
  throw std::invalid_argument("Actual allocated pool and prepared nonthreaded manager required");
 const auto backing=backend.memory.bytes();const auto address=reinterpret_cast<std::uintptr_t>(pool.data()),base=reinterpret_cast<std::uintptr_t>(backing.data());
 if(address<base || address-base>backing.size() || pool.size()>backing.size()-(address-base))
  throw std::invalid_argument("Pool storage is outside actual owned backend");
 const std::uint32_t references=1;const std::uint64_t zero=0;
 // Intermediate base table identity is overwritten by the concrete system-pool table.
 const std::uint64_t system_identity=0x025b9ca8;
 std::memcpy(pool.data()+8,&references,4);std::memcpy(pool.data()+0x10,&zero,8);std::memcpy(pool.data()+0x88,&zero,8);
 initialize_unowned_pool_storage(pool);
 std::memcpy(pool.data(),&system_identity,8);std::memcpy(manager.data()+0x10,&address,8);
 const std::uint32_t type=1;std::memcpy(pool.data()+0x1c,&type,4);
 const SystemPoolBootstrapContext lifecycle{false,false,-1,0,0,0};
 return activate_owned_system_pool(pool,backend,lifecycle);
}
}
