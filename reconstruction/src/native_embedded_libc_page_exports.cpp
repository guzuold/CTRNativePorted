#include "ctr/native_embedded_libc_page.h"
namespace { struct Owner{std::shared_ptr<ctr::NativeEmbeddedLibcPage> page;}; }
extern "C" {
__declspec(dllexport) void* ctr_embedded_page_load(const char* path){try{if(!path)return nullptr;return new Owner{ctr::NativeEmbeddedLibcPage::load(path)};}catch(...){return nullptr;}}
__declspec(dllexport) void* ctr_embedded_page_retain(void* handle){try{if(!handle)return nullptr;return new Owner{static_cast<Owner*>(handle)->page};}catch(...){return nullptr;}}
__declspec(dllexport) void ctr_embedded_page_release(void* handle){delete static_cast<Owner*>(handle);}
__declspec(dllexport) void* ctr_embedded_page_address(void* handle){return handle?static_cast<Owner*>(handle)->page->page().data():nullptr;}
__declspec(dllexport) void* ctr_embedded_page_translate(void* handle,unsigned long long logical,unsigned long long extent){try{if(!handle)return nullptr;return static_cast<Owner*>(handle)->page->translate(logical,static_cast<std::size_t>(extent)).data();}catch(...){return nullptr;}}
}
