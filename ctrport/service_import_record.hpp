// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "service_generation.hpp"
namespace ctrport {
struct ServiceImportRecord {
    std::string eboot_sha256,image_sha256,nid,library,module;
    std::uint64_t slot;
    std::uint32_t signature_id;
};
inline void validate_service_import_record(const ServiceImportRecord& record,
    const std::string& version,const std::string& image,std::uint64_t extent){
    auto sha=[](const std::string& text){
        if(text.size()!=64)return false;
        for(char c:text)if(!((c>='0' && c<='9') || (c>='a' && c<='f')))return false;
        return true;
    };
    if(!sha(record.eboot_sha256) || !sha(record.image_sha256) ||
       record.eboot_sha256!=version || record.image_sha256!=image)
        throw std::runtime_error("Service import version/image identity differs");
    if(extent<8 || record.slot>extent-8 || record.slot%8 ||
       record.nid.empty() || record.library.empty() || record.module.empty() || (record.signature_id!=1 && record.signature_id!=2 && record.signature_id!=9))
        throw std::runtime_error("Service import slot/identity/signature unsupported");
}
inline const void* resolve_service_import(const ServiceImportRecord& record,
    const ServiceGeneration& generation){
    if(generation.eboot_sha256!=record.eboot_sha256)
        throw std::runtime_error("Service generation retail version differs");
    for(const auto& entry:generation.entries){
        if(entry.nid==record.nid && entry.library==record.library && entry.module==record.module){
            if(entry.signature_id!=record.signature_id)
                throw std::runtime_error("Service generation signature differs");
            return entry.target;
        }
    }
    throw std::runtime_error("Service generation identity absent");
}
inline int service_import_record_control(){
    const std::string version(64,'a'),image(64,'b');static const int target=1;
    ServiceImportRecord good{version,image,"synthetic","test","test",8,1};
    validate_service_import_record(good,version,image,16);
    ServiceGeneration generation{std::make_shared<int>(1),{{"synthetic","test","test",&target,1}},1,version};
    if(resolve_service_import(good,generation)!=&target)throw std::runtime_error("Import record positive differs");
    unsigned rejected=0;
    auto check=[&](ServiceImportRecord record){
        try{validate_service_import_record(record,version,image,16);}
        catch(const std::runtime_error&){++rejected;}
    };
    auto bad=good;bad.eboot_sha256=image;check(bad);bad=good;bad.image_sha256=version;check(bad);
    bad=good;bad.slot=9;check(bad);bad=good;bad.slot=16;check(bad);
    bad=good;bad.signature_id=3;check(bad);bad=good;bad.nid.clear();check(bad);
    generation.eboot_sha256=image;
    try{resolve_service_import(good,generation);}catch(const std::runtime_error&){++rejected;}
    generation.eboot_sha256=version;generation.entries[0].signature_id=2;
    try{resolve_service_import(good,generation);}catch(const std::runtime_error&){++rejected;}
    generation.entries[0].signature_id=1;generation.entries[0].module="other";
    try{resolve_service_import(good,generation);}catch(const std::runtime_error&){++rejected;}
    if(rejected!=9)throw std::runtime_error("Import record rejections differ");
    return 0;
}
}
