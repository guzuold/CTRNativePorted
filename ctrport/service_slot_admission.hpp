// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "flat_loader.hpp"
#include "service_import_record.hpp"
namespace ctrport {
// Startup-only mutation: caller guarantees no thread can execute this import.
// This function does not suspend a running game or change image permissions.
inline void admit_service_slot(MappedImage& image,const ImageMetadata& metadata,
    const std::string& version,const ServiceImportRecord& record,
    const void* expected,const void* replacement){
    if(!image || !expected || !replacement)throw std::runtime_error("Service slot owner/target missing");
    std::uint64_t extent=0;bool backed=false;
    for(const auto& span:metadata.spans){
        if(span.second>extent)extent=span.second;
        if(span.second>=span.first && record.slot>=span.first && record.slot<=span.second &&
           8<=span.second-record.slot)backed=true;
    }
    validate_service_import_record(record,version,metadata.sha256,extent);
    if(!backed)throw std::runtime_error("Service slot outside backed image spans");
    unsigned matches=0;
    for(const auto& symbol:metadata.imports)if(symbol.slot==record.slot){
        if(symbol.type!=2 || symbol.addend || symbol.relocation!=7 || symbol.nid!=record.nid ||
           symbol.library!=record.library || symbol.module!=record.module)
            throw std::runtime_error("Service slot import identity/type differs");
        ++matches;
    }
    if(matches!=1)throw std::runtime_error("Service slot import absent/ambiguous");
    const auto base=reinterpret_cast<std::uintptr_t>(image.get());
    if(record.slot>UINTPTR_MAX-base || 8>UINTPTR_MAX-base-record.slot)
        throw std::runtime_error("Service slot host address overflow");
    auto* slot=image.get()+record.slot;MEMORY_BASIC_INFORMATION info{};
    if(VirtualQuery(slot,&info,sizeof(info))!=sizeof(info) || info.AllocationBase!=image.get() ||
       info.State!=MEM_COMMIT || (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
       !((info.Protect&0xff)==PAGE_READWRITE || (info.Protect&0xff)==PAGE_EXECUTE_READWRITE) ||
       reinterpret_cast<std::uintptr_t>(slot)-reinterpret_cast<std::uintptr_t>(info.BaseAddress)>info.RegionSize-8)
        throw std::runtime_error("Service slot owned writable range missing");
    const auto old=reinterpret_cast<std::uint64_t>(expected),next=reinterpret_cast<std::uint64_t>(replacement);
    std::uint64_t actual=0;std::memcpy(&actual,slot,8);
    if(actual!=old)throw std::runtime_error("Service slot expected pointer differs");
    std::memcpy(slot,&next,8);std::memcpy(&actual,slot,8);
    if(actual!=next)throw std::runtime_error("Service slot readback differs");
}
}
