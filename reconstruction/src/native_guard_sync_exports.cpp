#include "ctr/native_guard_sync.h"
namespace {struct Owner {std::shared_ptr<ctr::NativeGuardSync> sync;};}
extern "C" {
__declspec(dllexport) void* ctr_guard_sync_create(){try{return new Owner{std::make_shared<ctr::NativeGuardSync>()};}catch(...){return nullptr;}}
__declspec(dllexport) void* ctr_guard_sync_retain(void* h){try{if(!h)return nullptr;return new Owner{static_cast<Owner*>(h)->sync};}catch(...){return nullptr;}}
__declspec(dllexport) void ctr_guard_sync_release(void* h){delete static_cast<Owner*>(h);}
__declspec(dllexport) void* ctr_guard_sync_page(void* h){return h?static_cast<Owner*>(h)->sync->page().data():nullptr;}
__declspec(dllexport) int ctr_guard_sync_lock(void* h,void* cell){if(!h)return 22;auto s=static_cast<Owner*>(h)->sync;return s->lock(cell);}
__declspec(dllexport) int ctr_guard_sync_unlock(void* h,void* cell){if(!h)return 22;auto s=static_cast<Owner*>(h)->sync;return s->unlock(cell);}
__declspec(dllexport) int ctr_guard_sync_wait(void* h,void* cond,void* cell,unsigned timeout){if(!h)return 22;auto s=static_cast<Owner*>(h)->sync;return s->wait(cond,cell,timeout);}
__declspec(dllexport) int ctr_guard_sync_broadcast(void* h,void* cell){if(!h)return 22;auto s=static_cast<Owner*>(h)->sync;return s->broadcast(cell);}
__declspec(dllexport) unsigned ctr_guard_sync_waiters(void* h){return h?static_cast<Owner*>(h)->sync->waiters():0;}
}
