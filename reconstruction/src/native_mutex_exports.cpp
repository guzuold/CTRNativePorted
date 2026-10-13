#include "ctr/native_mutex.h"
#include <cstring>
extern "C" {
__declspec(dllexport) int ctr_native_address_sanitizer_enabled() {return ctr::native_address_sanitizer_enabled()?1:0;}
__declspec(dllexport) std::uint64_t ctr_native_stack_cookie() {return ctr::native_stack_cookie();}
__declspec(dllexport) int ctr_mutexattr_init(ctr::NativeMutexAttributes** p) {return ctr::mutexattr_init(p);}
__declspec(dllexport) int ctr_mutexattr_destroy(ctr::NativeMutexAttributes** p) {return ctr::mutexattr_destroy(p);}
__declspec(dllexport) int ctr_mutex_init(ctr::NativeMutex** p,ctr::NativeMutexAttributes** a,const char* name) {
    return ctr::mutex_init(p,a,name?std::string_view(name):std::string_view{});
}
__declspec(dllexport) int ctr_mutex_destroy(ctr::NativeMutex** p) {return ctr::mutex_destroy(p);}
__declspec(dllexport) int ctr_mutex_lock(ctr::NativeMutex* p) {return p?p->lock():22;}
__declspec(dllexport) int ctr_mutex_trylock(ctr::NativeMutex* p) {return p?p->try_lock():22;}
__declspec(dllexport) int ctr_mutex_unlock(ctr::NativeMutex* p) {return p?p->unlock():22;}
__declspec(dllexport) int ctr_mutex_private(ctr::NativeMutex* p) {if (!p) return 22;p->prefix.flags|=0x100;return 0;}
__declspec(dllexport) int ctr_mutex_prefix(ctr::NativeMutex* p,void* out) {
    if (!p||!out) return 22;
    std::memcpy(out,&p->prefix,36);return 0;
}
}
