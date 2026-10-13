#include "ctr/native_direct_memory.h"
extern "C" {
__declspec(dllexport) ctr::NativeDirectMemory* ctr_direct_create(std::uint64_t budget){try{return new ctr::NativeDirectMemory(budget);}catch(...){return nullptr;}}
__declspec(dllexport) void ctr_direct_destroy(ctr::NativeDirectMemory* p){delete p;}
__declspec(dllexport) std::uint64_t ctr_direct_size(ctr::NativeDirectMemory* p){return p?p->size():0;}
__declspec(dllexport) int ctr_direct_allocate(ctr::NativeDirectMemory* p,std::int64_t start,std::int64_t end,std::uint64_t length,std::uint64_t alignment,int type,std::int64_t* physical){return p?p->allocate(start,end,length,alignment,type,physical):22;}
__declspec(dllexport) int ctr_direct_map(ctr::NativeDirectMemory* p,void** address,std::uint64_t length,int protection,int flags,std::int64_t physical,std::uint64_t alignment){return p?p->map(address,length,protection,flags,physical,alignment):22;}
__declspec(dllexport) int ctr_direct_release(ctr::NativeDirectMemory* p){return p?p->release():22;}
}
