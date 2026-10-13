#include "ctr/typed_game_heap_callback.h"
#include <cstring>
#include <memory>
#include <new>
namespace {struct Handle{std::shared_ptr<ctr::NativeTypedGameHeapCallback> value;};}
extern "C" {
struct CtrTypedGameHeapCallbackStatus {std::uintptr_t dso_address,code_module;std::uint64_t callback_owner_references;int invoked,backend_released;};
__declspec(dllexport) void* ctr_typed_game_heap_callback_create(const char* original,const void* input,unsigned long long size){try{if(!original||!input||size!=431)return nullptr;return new Handle{std::make_shared<ctr::NativeTypedGameHeapCallback>(original,std::span<const std::byte>(static_cast<const std::byte*>(input),static_cast<std::size_t>(size))) };}catch(...){return nullptr;}}
__declspec(dllexport) void* ctr_typed_game_heap_callback_retain(void* owner){try{if(!owner)return nullptr;return new Handle{static_cast<Handle*>(owner)->value};}catch(...){return nullptr;}}
__declspec(dllexport) int ctr_typed_game_heap_callback_dispatch(void* owner,unsigned long long callback_id,unsigned long long argument_id,unsigned long long dso_id,void* output,unsigned long long output_size){try{if(!owner||!output||output_size!=50856)return 22;auto result=static_cast<Handle*>(owner)->value->dispatch(callback_id,argument_id,dso_id);std::memcpy(output,result.data(),result.size());return 0;}catch(...){return 22;}}
__declspec(dllexport) int ctr_typed_game_heap_callback_status(void* owner,CtrTypedGameHeapCallbackStatus* output){try{if(!owner||!output)return 22;const auto h=static_cast<Handle*>(owner);const auto s=h->value->status();*output={s.dso_address,s.code_module,static_cast<std::uint64_t>(h->value.use_count()),s.invoked?1:0,s.backend_released?1:0};return 0;}catch(...){return 22;}}
__declspec(dllexport) void ctr_typed_game_heap_callback_release(void* owner){delete static_cast<Handle*>(owner);}
}
