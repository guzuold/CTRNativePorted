// SPDX-License-Identifier: GPL-2.0-only
#define NOMINMAX
#include <windows.h>
#include "../../retail_map.hpp"
#include "distance_hook.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <exception>
#include <vector>
extern "C" std::uint64_t ctr_call_original_registers(void*,const std::uint64_t*,const std::uint64_t*,std::uint64_t*);
namespace ctrport::settings::geometry_controls {
namespace {
constexpr std::size_t site=0x34f200, size=17, trampoline=0x100;
constexpr unsigned char original[size]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x18};
template<class T>T read(const unsigned char* p,std::size_t at){T v;std::memcpy(&v,p+at,sizeof(v));return v;}
void absolute_jump(unsigned char* p,const void* target){const unsigned char op[]{0xff,0x25,0,0,0,0};std::memcpy(p,op,6);std::memcpy(p+6,&target,8);}
extern "C" void distance_callback(DistanceHook* owner,const unsigned long long* args) noexcept {owner->invoke(args);}
bool matching_scene(const std::uint64_t* args) noexcept {
    if (static_cast<unsigned char>(args[2])==0) return false;
    const auto* scene=reinterpret_cast<const unsigned char*>(args[0]);
    const auto count=read<std::int32_t>(scene,0x108);
    if(count<=0) return false;
    const auto* interned=reinterpret_cast<const unsigned char*>(args[1]);
    const auto name=read<std::uint64_t>(interned,0);
    const auto* records=read<const unsigned char*>(scene,0x118);
    for(std::int32_t i=0;i<count;++i)
        if(read<std::uint64_t>(records,static_cast<std::size_t>(i)*0x210+0x1f0)==name) return true;
    return false;
}
}
void DistanceHook::invoke(const unsigned long long* arguments) noexcept {
    std::array<std::uint64_t,6> args{};std::memcpy(args.data(),arguments,5*sizeof(std::uint64_t));
    std::array<float,5> distances{},fades{};
    if(matching_scene(args.data())) {
        const auto* d=reinterpret_cast<const float*>(args[3]);const auto* f=reinterpret_cast<const float*>(args[4]);
        bool valid=true;
        for(std::size_t i=0;i<5;++i) {
            std::memcpy(&distances[i],d+i,sizeof(float));std::memcpy(&fades[i],f+i,sizeof(float));
            valid=valid && std::isfinite(distances[i]) && std::isfinite(fades[i]) && distances[i]>=0 && fades[i]>0;
            // Preserve authored zero/sentinel and epsilon paths exactly. Only
            // ordinary positive, representable distances receive the override.
            const float nd=distances[i]*factor_,nf=fades[i]*factor_;
            valid=valid && std::isnormal(nd) && std::isnormal(nf) && nf>=0.0001f && std::isfinite(nd+nf);
        }
        if(valid) {
            for(std::size_t i=0;i<5;++i){distances[i]*=factor_;fades[i]*=factor_;}
            args[3]=reinterpret_cast<std::uintptr_t>(distances.data());args[4]=reinterpret_cast<std::uintptr_t>(fades.data());
        }
    }
    std::array<std::uint64_t,8> fp{};std::uint64_t result=0;
    ctr_call_original_registers(code_+trampoline,args.data(),fp.data(),&result);
}
std::unique_ptr<DistanceHook> install(unsigned char* base,std::size_t extent,const std::string& sha,DistanceScale selected) {
    if(selected!=DistanceScale::original && selected!=DistanceScale::half && selected!=DistanceScale::three_quarters)
        throw std::invalid_argument("invalid distance scale");
    if(selected==DistanceScale::original) return {};
    if(!retail_map::bound(sha) || !base || !retail_map::has(site) || retail_map::at(site)>extent || size>extent-retail_map::at(site))
        throw std::invalid_argument("unsupported distance hook image");
    MEMORY_BASIC_INFORMATION region{};
    if(!VirtualQuery(base+retail_map::at(site),&region,sizeof(region)) || region.State!=MEM_COMMIT ||
       (region.Protect & (PAGE_GUARD|PAGE_NOACCESS)) ||
       !(region.Protect & (PAGE_READWRITE|PAGE_EXECUTE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_WRITECOPY)) ||
       reinterpret_cast<std::uintptr_t>(base+retail_map::at(site))+size>reinterpret_cast<std::uintptr_t>(region.BaseAddress)+region.RegionSize)
        throw std::invalid_argument("distance hook requires writable bootstrap image");
    if(!retail_map::same(base+retail_map::at(site),site,original,size,true))throw std::invalid_argument("distance setter prologue mismatch");
    auto hook=std::unique_ptr<DistanceHook>(new DistanceHook());hook->image_=base;
    hook->factor_=selected==DistanceScale::half ? 0.5f : 0.75f;
    hook->code_=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!hook->code_)throw std::runtime_error("distance thunk allocation failed");
    // SysV arguments -> call-local array after Win64 shadow space. Subtracting
    // 88 bytes aligns the native Win64 call from the SysV entry alignment.
    std::vector<unsigned char> stub{0x48,0x83,0xec,0x58,
      0x48,0x89,0x7c,0x24,0x20,0x48,0x89,0x74,0x24,0x28,
      0x48,0x89,0x54,0x24,0x30,0x48,0x89,0x4c,0x24,0x38,
      0x4c,0x89,0x44,0x24,0x40,0x48,0x8d,0x54,0x24,0x20,0x48,0xb9};
    auto append_pointer=[&](const void* p){const auto* b=reinterpret_cast<const unsigned char*>(&p);stub.insert(stub.end(),b,b+8);};
    append_pointer(hook.get());stub.insert(stub.end(),{0x48,0xb8});append_pointer(reinterpret_cast<const void*>(&distance_callback));
    stub.insert(stub.end(),{0xff,0xd0,0x48,0x83,0xc4,0x58,0xc3});
    std::memcpy(hook->code_,stub.data(),stub.size());
    std::memcpy(hook->code_+trampoline,original,size);absolute_jump(hook->code_+trampoline+size,base+retail_map::at(site)+size);
    // UNWIND_INFO: v1, prolog4, one UWOP_ALLOC_SMALL(size88).
    const unsigned char unwind[]{1,4,1,0,4,0xa2,0,0};std::memcpy(hook->code_+0x200,unwind,sizeof(unwind));
    auto* function=reinterpret_cast<RUNTIME_FUNCTION*>(hook->code_+0x210);
    *function={0,static_cast<DWORD>(stub.size()),0x200};
    if(!RtlAddFunctionTable(function,1,reinterpret_cast<DWORD64>(hook->code_)))throw std::runtime_error("distance unwind registration failed");
    hook->unwind_registered_=true;
    DWORD previous=0;
    if(!VirtualProtect(hook->code_,4096,PAGE_EXECUTE_READ,&previous))throw std::runtime_error("distance thunk protection failed");
    FlushInstructionCache(GetCurrentProcess(),hook->code_,4096);
    absolute_jump(base+retail_map::at(site),hook->code_);std::memset(base+retail_map::at(site)+14,0x90,size-14);hook->installed_=true;
    FlushInstructionCache(GetCurrentProcess(),base+retail_map::at(site),size);
    return hook;
}
void DistanceHook::uninstall(){
    if(!installed_)return;
    std::array<unsigned char,size> expected{};absolute_jump(expected.data(),code_);std::memset(expected.data()+14,0x90,size-14);
    if(std::memcmp(image_+retail_map::at(site),expected.data(),size))throw std::runtime_error("distance hook altered by another patch");
    DWORD old=0;if(!VirtualProtect(image_+retail_map::at(site),size,PAGE_EXECUTE_READWRITE,&old))throw std::runtime_error("distance unhook protection failed");
    std::memcpy(image_+retail_map::at(site),original,size);FlushInstructionCache(GetCurrentProcess(),image_+retail_map::at(site),size);
    DWORD ignored=0;if(!VirtualProtect(image_+retail_map::at(site),size,old,&ignored))throw std::runtime_error("distance unhook restore protection failed");
    installed_=false;
}
DistanceHook::~DistanceHook(){
    try{uninstall();}catch(...){std::terminate();}
    if(unwind_registered_)RtlDeleteFunctionTable(reinterpret_cast<RUNTIME_FUNCTION*>(code_+0x210));
    if(code_)VirtualFree(code_,0,MEM_RELEASE);
}
}
