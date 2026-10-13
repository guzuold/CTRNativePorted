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
inline int service_slot_admission_control(){
    MappedImage image(static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE)));
    if(!image)throw std::runtime_error("Slot fixture allocation failed");
    const std::string pin(64,'a');ServiceImportRecord record{pin,pin,"synthetic","test","test",8,1};
    ImageMetadata metadata{};metadata.sha256=pin;metadata.spans={{0,4096}};
    OriginalSymbol symbol{};symbol.nid=record.nid;symbol.library=record.library;symbol.module=record.module;
    symbol.type=2;symbol.relocation=7;symbol.slot=8;metadata.imports.push_back(symbol);
    const auto old=reinterpret_cast<const void*>(std::uintptr_t(0x1000));
    const auto next=reinterpret_cast<const void*>(std::uintptr_t(0x2000));
    auto value=reinterpret_cast<std::uint64_t>(old);std::memcpy(image.get()+8,&value,8);
    admit_service_slot(image,metadata,pin,record,old,next);
    admit_service_slot(image,metadata,pin,record,next,old); // checked rollback.
    unsigned rejected=0;auto check=[&](ImageMetadata candidate,ServiceImportRecord item,const void* expected){
        try{admit_service_slot(image,candidate,pin,item,expected,next);}catch(const std::runtime_error&){++rejected;}
    };
    check(metadata,record,next);auto bad=metadata;bad.imports[0].nid="other";check(bad,record,old);
    bad=metadata;bad.imports.push_back(symbol);check(bad,record,old);
    bad=metadata;bad.imports.clear();check(bad,record,old);
    bad=metadata;bad.imports[0].addend=1;check(bad,record,old);
    bad=metadata;bad.spans={{16,4096}};check(bad,record,old);
    DWORD previous=0;if(!VirtualProtect(image.get(),4096,PAGE_READONLY,&previous))throw std::runtime_error("Slot fixture protect failed");
    check(metadata,record,old);
    std::memcpy(&value,image.get()+8,8);
    if(rejected!=7 || value!=reinterpret_cast<std::uint64_t>(old))throw std::runtime_error("Slot negative changed target");
    return 0;
}
}
