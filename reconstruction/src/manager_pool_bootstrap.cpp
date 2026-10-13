#include "ctr/manager_pool_bootstrap.h"
#include "ctr/game_global_malloc.h"
#include "ctr/pool_manager.h"
#include <cstring>
namespace ctr {
SystemPoolAllocation begin_warm_manager_pool_bootstrap(std::span<std::byte> descriptor,
 std::span<const std::byte> previous_manager,std::span<std::byte> previous_pool,
 SystemPoolBackend& backend,bool poison,std::uint64_t& publication){
 auto manager=begin_warm_descriptor_bootstrap(descriptor,previous_manager,previous_pool,backend,poison,publication);
 const PreparedGameGlobalMalloc binding{&backend,0x01437990,poison};
 auto pool=allocate_game_global_top(binding,176);
 const std::uint64_t zero=0,table=0x025b9ca8,address=pool.address_token;
 const std::uint32_t references=1,type=1;
 std::memcpy(pool.payload.data()+8,&references,4);
 std::memcpy(pool.payload.data()+0x10,&zero,8);
 std::memcpy(pool.payload.data()+0x88,&zero,8);
 initialize_unowned_pool_storage(pool.payload);
 std::memcpy(pool.payload.data(),&table,8);
 std::memcpy(manager.payload.data()+0x10,&address,8);
 std::memcpy(pool.payload.data()+0x1c,&type,4);
 // Deliberately stop before014d7c30: new registry lifecycle remains missing.
 return manager;
}
}
