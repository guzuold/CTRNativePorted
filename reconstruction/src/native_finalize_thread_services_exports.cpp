#include "ctr/native_finalize_thread_services.h"
extern "C" {
__declspec(dllexport) ctr::NativeFinalizeThreadServices* ctr_finalize_services_create(unsigned long long address){return ctr::create_finalize_thread_services(address);}
__declspec(dllexport) int ctr_finalize_services_release(ctr::NativeFinalizeThreadServices* owner){return ctr::release_finalize_thread_services(owner);}
__declspec(dllexport) int ctr_finalize_thread_self(ctr::NativeFinalizeThreadServices* owner,unsigned long long* token){return ctr::finalize_thread_self(owner,token);}
__declspec(dllexport) int ctr_finalize_mutex_lock(ctr::NativeFinalizeThreadServices* owner,unsigned long long address){return ctr::finalize_mutex_lock(owner,address);}
__declspec(dllexport) int ctr_finalize_mutex_unlock(ctr::NativeFinalizeThreadServices* owner,unsigned long long address){return ctr::finalize_mutex_unlock(owner,address);}
}
