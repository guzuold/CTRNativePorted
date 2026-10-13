#include "ctr/native_mutex.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <string>
#include <new>
extern "C" std::uintptr_t __security_cookie;
namespace ctr {
struct NativeMutexBackend {SRWLOCK lock=SRWLOCK_INIT;std::string name;};
NativeMutex::NativeMutex(std::string_view name):backend_(std::make_unique<NativeMutexBackend>()) {backend_->name=name;}
NativeMutex::~NativeMutex()=default;
int NativeMutex::lock() noexcept {
    if ((prefix.flags&255)!=1 || prefix.protocol) return 22;
    const auto owner=static_cast<std::uint64_t>(GetCurrentThreadId());
    if (prefix.owner.load()==owner) return 11;
    AcquireSRWLockExclusive(&backend_->lock);prefix.owner.store(owner);return 0;
}
int NativeMutex::try_lock() noexcept {
    if ((prefix.flags&255)!=1 || prefix.protocol) return 22;
    if (!TryAcquireSRWLockExclusive(&backend_->lock)) return 16;
    prefix.owner.store(GetCurrentThreadId());return 0;
}
int NativeMutex::unlock() noexcept {
    if (prefix.owner.load()!=GetCurrentThreadId()) return 1;
    prefix.owner.store(0);ReleaseSRWLockExclusive(&backend_->lock);return 0;
}
int mutexattr_init(NativeMutexAttributes** attr) noexcept {
    if (!attr) return 22;
    auto* value=new(std::nothrow) NativeMutexAttributes;
    if (!value) return 12;
    *attr=value;return 0;
}
int mutexattr_destroy(NativeMutexAttributes** attr) noexcept {
    if (!attr||!*attr) return 22;
    delete *attr;*attr=nullptr;return 0;
}
int mutex_init(NativeMutex** out,NativeMutexAttributes* const* attr,std::string_view name) noexcept {
    if (!out || (attr&&(!*attr || (*attr)->type!=1 || (*attr)->protocol!=0))) return 22;
    try {*out=new NativeMutex(name);return 0;} catch (...) {return 12;}
}
int mutex_destroy(NativeMutex** mutex) noexcept {
    if (!mutex||!*mutex) return 22;
    if ((*mutex)->prefix.owner.load()) return 16;
    delete *mutex;*mutex=nullptr;return 0;
}
bool native_address_sanitizer_enabled() noexcept {
#if defined(__SANITIZE_ADDRESS__)
    return true;
#else
    return false;
#endif
}
std::uint64_t native_stack_cookie() noexcept {return __security_cookie;}
}
