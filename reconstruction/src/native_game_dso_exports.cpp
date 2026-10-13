#include "ctr/native_game_dso.h"
namespace { struct Owner{std::shared_ptr<ctr::NativeGameDso> page;}; }
extern "C" {
__declspec(dllexport) void* ctr_game_dso_load(const char* path){try{if(!path)return nullptr;return new Owner{ctr::NativeGameDso::load(path)};}catch(...){return nullptr;}}
__declspec(dllexport) void* ctr_game_dso_retain(void* handle){try{if(!handle)return nullptr;return new Owner{static_cast<Owner*>(handle)->page};}catch(...){return nullptr;}}
__declspec(dllexport) void ctr_game_dso_release(void* handle){delete static_cast<Owner*>(handle);}
__declspec(dllexport) void* ctr_game_dso_address(void* handle){return handle?static_cast<Owner*>(handle)->page->page().data():nullptr;}
__declspec(dllexport) void* ctr_game_dso_translate(void* handle,unsigned long long logical,unsigned long long extent){try{if(!handle)return nullptr;return static_cast<Owner*>(handle)->page->translate(logical,static_cast<std::size_t>(extent)).data();}catch(...){return nullptr;}}
}
