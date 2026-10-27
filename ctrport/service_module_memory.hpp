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
}
