#include "ctr/descriptor_bootstrap_prefix.h"
#include "ctr/descriptor_manager.h"
#include <stdexcept>
namespace ctr {
MspaceAllocation begin_cold_descriptor_bootstrap(std::span<std::byte> descriptor,
 const PreparedGameGlobalMalloc& binding,std::uint64_t& publication){
 if(descriptor.size()!=176||descriptor[0]!=std::byte{0})throw std::invalid_argument("Cold descriptor176 required");
 mark_dispatch_descriptor_bootstrap_started(descriptor);
 auto allocation=allocate_game_global_top(binding,112);
 construct_allocated_descriptor_manager(descriptor,allocation.payload,publication);
 return allocation;
}
SystemPoolAllocation begin_warm_descriptor_bootstrap(std::span<std::byte> descriptor,
 std::span<const std::byte> current_manager,std::span<std::byte> pool,
 SystemPoolBackend& backend,bool poison,std::uint64_t& publication){
 // Validate the supported selector contract before the first descriptor mutation.
 (void)select_descriptor_manager_pool(descriptor,current_manager,1);
 mark_dispatch_descriptor_bootstrap_started(descriptor);
 auto allocation=allocate_warm_descriptor_manager(descriptor,current_manager,pool,backend,poison);
 construct_allocated_descriptor_manager(descriptor,allocation.payload,publication);
 return allocation;
}
}
