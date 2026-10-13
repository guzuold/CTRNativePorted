#include "ctr/native_procparam.h"
#include <memory>
namespace {struct Lease {std::shared_ptr<const ctr::NativeProcParam> owner;};}
extern "C" {
__declspec(dllexport) void* ctr_procparam_load(const wchar_t* self,const wchar_t* flat) noexcept {try{if(!self||!flat)return nullptr;return new Lease{ctr::NativeProcParam::load(self,flat)};}catch(...){return nullptr;}}
__declspec(dllexport) void* ctr_procparam_retain(void* lease) noexcept {try{return lease?new Lease{static_cast<Lease*>(lease)->owner}:nullptr;}catch(...){return nullptr;}}
__declspec(dllexport) void ctr_procparam_release(void* lease) noexcept {delete static_cast<Lease*>(lease);}
__declspec(dllexport) std::uint64_t ctr_procparam_query(void* lease) noexcept {return lease?static_cast<Lease*>(lease)->owner->query_logical_root():0;}
__declspec(dllexport) const void* ctr_procparam_page(void* lease,std::uint64_t address) noexcept {try{return lease?static_cast<Lease*>(lease)->owner->page(address).data():nullptr;}catch(...){return nullptr;}}
}
