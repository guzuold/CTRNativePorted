#include "ctr/game_global_malloc.h"
#include <stdexcept>
namespace ctr {
MspaceAllocation allocate_game_global_top(const PreparedGameGlobalMalloc& binding,std::uint64_t request){
 if(!binding.backend||binding.original_malloc_target!=0x01437990||!request||request>512)throw std::invalid_argument("Prepared actual game malloc target and request1..512 required");
 auto& b=*binding.backend;auto backing=b.memory.bytes();
 if(b.capacity!=0x4000||backing.size()!=0x4000||reinterpret_cast<std::uint64_t>(backing.data())!=0x60000000||b.heap.header_token!=0x60000020)throw std::invalid_argument("Actual preconfigured System Pool heap required");
 // Game callback loads the published actual heap and tail-dispatches MspaceMalloc.
 const MspaceTopAllocationContext context{reinterpret_cast<std::uint64_t>(backing.data()),b.heap.header_token,binding.poison,b.heap.mutex.get()};
 return allocate_mspace_top(backing,context,request);
}
}

