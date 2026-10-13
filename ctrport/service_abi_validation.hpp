// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "service_abi.h"
#include "service_generation.hpp"
#include <cstring>
namespace ctrport {
inline std::vector<ServiceGeneration::Entry> validate_service_manifest(
    const CtrServiceManifest& manifest,const std::string& pin) {
    if(manifest.size!=sizeof(manifest) || manifest.version!=CTRPORT_SERVICE_ABI_VERSION ||
       manifest.entry_size!=sizeof(CtrServiceEntry) || !manifest.entry_count ||
       manifest.entry_count>4096 || !manifest.entries)
        throw std::runtime_error("Service ABI schema unsupported");
    for(auto value:manifest.reserved)if(value)throw std::runtime_error("Service ABI reserved field set");
    if(pin.size()!=64 || manifest.eboot_sha256[64] || std::memcmp(manifest.eboot_sha256,pin.data(),64))
        throw std::runtime_error("Service ABI retail identity differs");
    std::vector<ServiceGeneration::Entry> result;
    auto bounded=[](const char* data,std::size_t capacity){
        const auto end=static_cast<const char*>(std::memchr(data,0,capacity));
        if(!end || end==data)throw std::runtime_error("Service ABI string invalid");
        return std::string(data,end);
    };
    for(std::uint32_t i=0;i<manifest.entry_count;++i){
        const auto& entry=manifest.entries[i];
        if(entry.size!=sizeof(entry) || !entry.signature_id || !entry.win64_adapter)
            throw std::runtime_error("Service ABI adapter incomplete");
        // Integer3: signature1 direct result; signature2 explicit transport/output.
        if(entry.signature_id!=1 && entry.signature_id!=2 && entry.signature_id!=9)throw std::runtime_error("Service ABI signature unsupported");
        ServiceGeneration::Entry next{bounded(entry.nid,sizeof(entry.nid)),
            bounded(entry.library,sizeof(entry.library)),bounded(entry.module,sizeof(entry.module)),entry.win64_adapter,entry.signature_id};
        for(const auto& prior:result)if(prior.nid==next.nid && prior.library==next.library && prior.module==next.module)
            throw std::runtime_error("Service ABI duplicate identity");
        result.push_back(std::move(next));
    }
    return result;
}
inline int service_abi_control(){
    const std::string pin(64,'a');static const int target=1;
    CtrServiceEntry entry{};entry.size=sizeof(entry);entry.signature_id=1;
    std::memcpy(entry.nid,"synthetic",10);std::memcpy(entry.library,"test",5);std::memcpy(entry.module,"test",5);entry.win64_adapter=&target;
    CtrServiceManifest good{};good.size=sizeof(good);good.version=1;good.entry_size=sizeof(entry);good.entry_count=1;good.entries=&entry;
    std::memcpy(good.eboot_sha256,pin.data(),64);
    if(validate_service_manifest(good,pin).size()!=1)throw std::runtime_error("Service ABI positive differs");
    unsigned rejected=0;
    auto check=[&](CtrServiceManifest candidate){try{validate_service_manifest(candidate,pin);}catch(const std::runtime_error&){++rejected;}};
    auto bad=good;bad.version=2;check(bad);bad=good;bad.entry_size=1;check(bad);
    bad=good;bad.entries=nullptr;check(bad);bad=good;bad.entry_count=4097;check(bad);
    bad=good;bad.reserved[0]=1;check(bad);bad=good;bad.eboot_sha256[0]='b';check(bad);
    entry.signature_id=3;check(good);entry.signature_id=1;entry.win64_adapter=nullptr;check(good);entry.win64_adapter=&target;
    std::memset(entry.nid,'x',sizeof(entry.nid));check(good);
    if(rejected!=9)throw std::runtime_error("Service ABI rejection controls differ");
    return 0;
}
}
