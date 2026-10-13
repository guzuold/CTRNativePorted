#include "ctr/global_heap_mutex.h"
#include "ctr/heap_full_name.h"
#include <stdexcept>
namespace ctr {
GlobalHeapMutexStage initialize_global_heap_mutex_stage(std::span<std::byte> header,std::uint64_t& selected){
 const auto pointer=reinterpret_cast<std::uint64_t>(header.data());
 const auto cell=reinterpret_cast<std::uint64_t>(&selected);
 if(header.size()<0x380||pointer%8||(selected!=0&&selected!=pointer)|| (cell>=pointer&&cell<pointer+header.size()) || (cell<pointer&&pointer-cell<8))throw std::invalid_argument("Actual header and zero/matching external selection required");
 if(!selected)selected=pointer;
 GlobalHeapMutexStage stage;
 make_heap_pointer_name(stage.name,"SceLibcHeap",selected);
 stage.mutex=initialize_mspace_mutex(header,0,std::string_view(reinterpret_cast<const char*>(stage.name.data()),26));
 if(!stage.mutex)throw std::runtime_error("Native mutex initialization failed; original failure path excluded");
 return stage;
}
}
