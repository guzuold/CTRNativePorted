#include "ctr/game_global_free.h"
#include "ctr/mspace_free.h"
#include <stdexcept>
namespace ctr {
std::uint32_t free_game_global_top(const PreparedGameGlobalFree& binding,std::uint64_t pointer){
 if(!binding.backend||binding.original_free_target!=0x014379b0)throw std::invalid_argument("Prepared actual game free callback required");
 auto& b=*binding.backend;auto backing=b.memory.bytes();
 if(b.capacity!=0x4000||backing.size()!=0x4000||reinterpret_cast<std::uint64_t>(backing.data())!=0x60000000||b.heap.header_token!=0x60000020)throw std::invalid_argument("Actual preconfigured System Pool required");
 free_mspace_top(backing,{reinterpret_cast<std::uint64_t>(backing.data()),b.heap.header_token,binding.poison,b.heap.mutex.get()},pointer);
 return 0;
}
}
