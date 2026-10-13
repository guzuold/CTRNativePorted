#include "ctr/new_pool_factory_request.h"
#include <cstring>
#include <stdexcept>
namespace ctr {
PendingPoolFactoryRequest begin_new_pool_factory_request(std::span<std::byte> descriptor,
 std::span<const std::byte> previous_manager,std::span<std::byte> previous_pool,
 SystemPoolBackend& backend,bool poison,std::uint64_t& publication){
 auto manager=begin_new_pool_activation_prefix(descriptor,previous_manager,previous_pool,backend,poison,publication);
 std::uint64_t owner,pool;std::uint32_t pending;
 std::memcpy(&owner,descriptor.data()+0x70,8);
 std::memcpy(&pending,descriptor.data()+0x18,4);
 std::memcpy(&pool,manager.payload.data()+0x10,8);
 if(owner||pending)throw std::invalid_argument("Fresh null mutex/cold pending count required");
 // Actual014ecd5a increments descriptor pending count before call014ecc80.
 ++pending;std::memcpy(descriptor.data()+0x18,&pending,4);
 return {manager,0x026f1ae8,0x01ad7f4a,pool,PoolFactoryDependency::IgMutexMetadataInitializerMissing};
}
}
