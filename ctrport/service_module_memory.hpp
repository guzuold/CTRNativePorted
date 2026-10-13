// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
#include <stdexcept>
#include "service_abi_validation.hpp"
namespace ctrport {
// The owner must retain its loaded module during every check and subsequent use.
// This is a trusted DLL boundary, not isolation from malicious executable code.
inline void require_service_module_range(HMODULE owner,const void* pointer,std::size_t size,bool executable){
    const auto begin=reinterpret_cast<std::uintptr_t>(pointer);
    if(!owner || !begin || !size || size>UINTPTR_MAX-begin)
        throw std::runtime_error("Service module range invalid");
    const auto end=begin+size;
    for(auto cursor=begin;cursor<end;){
        MEMORY_BASIC_INFORMATION info{};
        if(VirtualQuery(reinterpret_cast<const void*>(cursor),&info,sizeof(info))!=sizeof(info) ||
           info.State!=MEM_COMMIT || info.Type!=MEM_IMAGE || info.AllocationBase!=owner ||
           (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))
            throw std::runtime_error("Service pointer outside owned readable image");
        const DWORD protection=info.Protect&0xff;
        const bool read=protection==PAGE_READONLY || protection==PAGE_READWRITE ||
            protection==PAGE_WRITECOPY || protection==PAGE_EXECUTE_READ ||
            protection==PAGE_EXECUTE_READWRITE || protection==PAGE_EXECUTE_WRITECOPY;
        const bool execute=protection==PAGE_EXECUTE_READ || protection==PAGE_EXECUTE_READWRITE ||
            protection==PAGE_EXECUTE_WRITECOPY;
        if(!read || (executable && !execute))throw std::runtime_error("Service image protection differs");
        const auto region=reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        if(info.RegionSize>UINTPTR_MAX-region || region+info.RegionSize<=cursor)
            throw std::runtime_error("Service image extent invalid");
        cursor=(region+info.RegionSize<end)?region+info.RegionSize:end;
    }
}
// Copy the schema and entries only after validating the entire owned ranges.
// DLLs must keep their manifest immutable while the owner is retained.
inline std::vector<ServiceGeneration::Entry> snapshot_service_manifest(
    HMODULE owner,const CtrServiceManifest* source,const std::string& pin){
    require_service_module_range(owner,source,sizeof(CtrServiceManifest),false);
    CtrServiceManifest manifest{};
    std::memcpy(&manifest,source,sizeof(manifest));
    if(manifest.entry_count<1 || manifest.entry_count>4096 ||
       manifest.entry_size!=sizeof(CtrServiceEntry))
        throw std::runtime_error("Service manifest entry extent unsupported");
    const auto bytes=static_cast<std::size_t>(manifest.entry_count)*sizeof(CtrServiceEntry);
    require_service_module_range(owner,manifest.entries,bytes,false);
    std::vector<CtrServiceEntry> entries(manifest.entry_count);
    std::memcpy(entries.data(),manifest.entries,bytes);
    manifest.entries=entries.data();
    auto result=validate_service_manifest(manifest,pin);
    for(const auto& entry:result)require_service_module_range(owner,entry.target,1,true);
    return result;
}
inline int service_manifest_snapshot_control(){
    const auto owner=GetModuleHandleW(nullptr);const std::string pin(64,'a');
    static CtrServiceEntry entry{};entry.size=sizeof(entry);entry.signature_id=1;
    std::memcpy(entry.nid,"synthetic",10);std::memcpy(entry.library,"test",5);
    std::memcpy(entry.module,"test",5);
    entry.win64_adapter=reinterpret_cast<const void*>(&service_manifest_snapshot_control);
    static CtrServiceManifest manifest{};manifest.size=sizeof(manifest);manifest.version=1;
    manifest.entry_size=sizeof(entry);manifest.entry_count=1;manifest.entries=&entry;
    std::memcpy(manifest.eboot_sha256,pin.data(),64);
    const auto result=snapshot_service_manifest(owner,&manifest,pin);
    if(result.size()!=1 || result[0].target!=entry.win64_adapter)
        throw std::runtime_error("Manifest snapshot positive differs");
    unsigned rejected=0;
    auto check=[&](const CtrServiceManifest* source){
        try{snapshot_service_manifest(owner,source,pin);}catch(const std::runtime_error&){++rejected;}
    };
    check(nullptr);
    const auto stack=manifest;check(&stack);
    manifest.entries=reinterpret_cast<const CtrServiceEntry*>(UINTPTR_MAX-1);check(&manifest);
    manifest.entries=&entry;manifest.entry_count=4097;check(&manifest);manifest.entry_count=1;
    entry.win64_adapter=&manifest;check(&manifest);
    if(rejected!=5)throw std::runtime_error("Manifest snapshot rejections differ");
    return 0;
}
inline int service_module_memory_control(){
    const auto owner=GetModuleHandleW(nullptr);
    static const std::uint64_t data=42;
    const auto code=reinterpret_cast<const void*>(&service_module_memory_control);
    require_service_module_range(owner,&data,sizeof(data),false);
    require_service_module_range(owner,code,1,true);
    unsigned rejected=0;
    auto check=[&](HMODULE module,const void* ptr,std::size_t size,bool execute){
        try{require_service_module_range(module,ptr,size,execute);}catch(const std::runtime_error&){++rejected;}
    };
    check(owner,&data,sizeof(data),true);
    check(owner,nullptr,1,false);
    check(owner,&data,0,false);
    check(owner,reinterpret_cast<const void*>(UINTPTR_MAX-1),8,false);
    check(GetModuleHandleW(L"kernel32.dll"),code,1,true);
    const std::uint64_t stack=1;check(owner,&stack,sizeof(stack),false);
    if(rejected!=6)throw std::runtime_error("Service module memory controls differ");
    return 0;
}
}
