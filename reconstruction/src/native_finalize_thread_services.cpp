#include "ctr/native_finalize_thread_services.h"
#include "ctr/native_mutex.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <memory>
#include <new>
namespace ctr {
struct NativeFinalizeThreadServices {std::uint64_t logical_mutex{};std::unique_ptr<NativeMutex> mutex;};
NativeFinalizeThreadServices* create_finalize_thread_services(std::uint64_t logical_mutex){
 if(logical_mutex!=0x50124268)return nullptr;
 try{auto owner=std::make_unique<NativeFinalizeThreadServices>();owner->logical_mutex=logical_mutex;owner->mutex=std::make_unique<NativeMutex>("bounded cxa finalizer lock");return owner.release();}catch(...){return nullptr;}
}
int release_finalize_thread_services(NativeFinalizeThreadServices* owner) noexcept {
 if(!owner||!owner->mutex)return 22;
 if(owner->mutex->prefix.owner.load()!=0)return 16;
 delete owner;return 0;
}
int finalize_thread_self(NativeFinalizeThreadServices* owner,std::uint64_t* thread_token) noexcept {
 if(!owner||!thread_token)return 22;
 const auto thread=static_cast<std::uint64_t>(GetCurrentThreadId());
 if(!thread)return 22;
 *thread_token=thread;return 0;
}
int finalize_mutex_lock(NativeFinalizeThreadServices* owner,std::uint64_t logical_mutex) noexcept {
 if(!owner||!owner->mutex||logical_mutex!=owner->logical_mutex)return 22;
 return owner->mutex->lock();
}
int finalize_mutex_unlock(NativeFinalizeThreadServices* owner,std::uint64_t logical_mutex) noexcept {
 if(!owner||!owner->mutex||logical_mutex!=owner->logical_mutex)return 22;
 return owner->mutex->unlock();
}
}
