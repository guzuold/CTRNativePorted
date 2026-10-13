#include "ctr/new_pool_activation_prefix.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
SystemPoolAllocation begin_new_pool_activation_prefix(std::span<std::byte> descriptor,
 std::span<const std::byte> previous_manager,std::span<std::byte> previous_pool,
 SystemPoolBackend& backend,bool poison,std::uint64_t& publication){
 auto manager=begin_warm_manager_pool_bootstrap(descriptor,previous_manager,previous_pool,backend,poison,publication);
 std::uint64_t address;std::memcpy(&address,manager.payload.data()+0x10,8);
 auto backing=backend.memory.bytes();const auto base=reinterpret_cast<std::uint64_t>(backing.data());
 if(address<base||address-base>backing.size()||176>backing.size()-(address-base))throw std::invalid_argument("Actual new pool outside backend");
 auto pool=backing.subspan(static_cast<std::size_t>(address-base),176);
 // Fresh concrete System Pool getter014d7dd0 returns0; stored alignment is0.
 const std::uint32_t alignment=8;std::memcpy(pool.data()+0x30,&alignment,4);
 std::memset(pool.data()+0x50,0,48);
 if(pool[0x39]!=std::byte{0})pool[0x38]=std::byte{1};
 // Original owner is null, warm manager nonthreaded and selector1 resolves this
 // pool. Next015b52d0 is a real factory dependency, deliberately left missing.
 return manager;
}
}
