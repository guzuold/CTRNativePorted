#include "ctr/native_atexit_bss.h"
namespace { struct Owner{std::shared_ptr<ctr::NativeAtexitBss> page;}; }
extern "C" {
__declspec(dllexport) void* ctr_atexit_bss_load(const char* path){try{if(!path)return nullptr;return new Owner{ctr::NativeAtexitBss::load(path)};}catch(...){return nullptr;}}
__declspec(dllexport) void* ctr_atexit_bss_retain(void* handle){try{if(!handle)return nullptr;return new Owner{static_cast<Owner*>(handle)->page};}catch(...){return nullptr;}}
__declspec(dllexport) void ctr_atexit_bss_release(void* handle){delete static_cast<Owner*>(handle);}
__declspec(dllexport) void* ctr_atexit_bss_address(void* handle){return handle?static_cast<Owner*>(handle)->page->page().data():nullptr;}
__declspec(dllexport) void* ctr_atexit_bss_translate(void* handle,unsigned long long logical,unsigned long long extent){try{if(!handle)return nullptr;return static_cast<Owner*>(handle)->page->translate(logical,static_cast<std::size_t>(extent)).data();}catch(...){return nullptr;}}
}
