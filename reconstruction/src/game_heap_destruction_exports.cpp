#include "ctr/game_heap_destructor.h"
extern "C" {
__declspec(dllexport) int ctr_direct_release_region(ctr::NativeDirectMemory* owner,
 std::int64_t physical,std::uint64_t length){return owner?ctr::release_direct_memory_region(*owner,physical,length):22;}
}
