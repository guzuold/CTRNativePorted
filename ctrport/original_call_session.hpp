// SPDX-License-Identifier: GPL-2.0-only
#pragma once
// Calls declared original functions of the retail images (CTR_ORIGINAL_CALL_MANIFEST) for
// the runtime's control modes. Formerly tools/harness/original_call_session.hpp.
#include "original_call.hpp"
#include "original_register_call.hpp"
#include <algorithm>
#include "original_binding.hpp"
#include "missing_import.hpp"
#include <cstdlib>
#include "bank_dispatch.hpp"
#include <cstdio>
namespace ctrport {
// One active call per thread; missing services terminate the isolated probe.
inline thread_local BankCall* session_bank_call=nullptr;
inline LONG CALLBACK session_missing_handler(EXCEPTION_POINTERS* exception) {
    auto* call=session_bank_call;
    if(!call || capture_bank_call(exception,call)!=EXCEPTION_EXECUTE_HANDLER)
        return EXCEPTION_CONTINUE_SEARCH;
    const auto& bank=call->data_access?*call->data:*call->functions;
    const auto& identity=bank.records[call->index].identity;
    std::fprintf(stderr,"Missing original import NID=%s library=%s image_sha256=%s caller=0x%llx\n",
                 identity.nid.c_str(),identity.library.c_str(),identity.image_sha256.c_str(),
                 static_cast<unsigned long long>(call->caller));
    std::fflush(stderr);ExitProcess(4);
}
inline std::uint64_t invoke_session_target(void* target,std::uint64_t first,std::uint64_t second,
                                          const MissingTrapBank& functions,const MissingTrapBank& data) {
    if(session_bank_call) throw std::runtime_error("Nested original session call unsupported");
    BankCall context;context.functions=&functions;context.data=&data;
    auto handler=AddVectoredExceptionHandler(1,session_missing_handler);
    if(!handler) throw std::runtime_error("Original session exception handler unavailable");
    session_bank_call=&context;
    const auto result=ctr_call_original_integer2(target,first,second);
    session_bank_call=nullptr;RemoveVectoredExceptionHandler(handler);
    return result;
}
class OriginalCallSession {
    std::vector<MappedImage> images;
    std::vector<ImageMetadata> metadata;
    std::map<std::string,OriginalCallDeclaration> calls;
    struct Signature {unsigned integers;std::string xmm;char result;};
    std::map<std::string,Signature> signatures;
    MissingTrapBank functions,data;
public:
    OriginalCallSession() {
        char path[32768];
        const auto length=GetEnvironmentVariableA("CTR_ORIGINAL_CALL_MANIFEST",path,sizeof(path));
        if(!length || length>=sizeof(path)) throw std::runtime_error("Original call manifest missing");
        std::ifstream input(path);std::string magic,eboot;
        unsigned count=0;input>>magic>>eboot>>count;
        if((magic!="CTR_CALLS_1" && magic!="CTR_CALLS_2") || count==0 || count>32) throw std::runtime_error("Original call manifest invalid");
        for(unsigned i=0;i<count;++i) {
            std::string filename;input>>std::quoted(filename);ImageMetadata info;
            images.push_back(load_retail_image(filename.c_str(),false,&info));metadata.push_back(std::move(info));
        }
        if(metadata.front().sha256!=eboot) throw std::runtime_error("Original session eboot SHA differs");
        bind_original_images(images,metadata);
        functions=install_missing_traps(images,metadata);data=install_missing_data_guards(images,metadata);
        unsigned callees=0;input>>callees;
        if(callees==0 || callees>1024) throw std::runtime_error("Original callee count invalid");
        for(unsigned i=0;i<callees;++i) {
            std::string id,sha;std::uint64_t address=0,size=0;input>>id>>sha>>address>>size;
            if(!input || size==0 || calls.count(id)) throw std::runtime_error("Original callee declaration invalid");
            if(magic=="CTR_CALLS_2") {
                Signature signature{};input>>signature.integers>>signature.xmm>>signature.result;
                if(!input || signature.integers>6 || (signature.xmm!="-" && (signature.xmm.size()>8 || signature.xmm.find_first_not_of("fd")!=std::string::npos)) || (signature.result!='i' && signature.result!='f' && signature.result!='d')) throw std::runtime_error("Original register signature invalid");
                if(signature.xmm=="-") signature.xmm.clear();
                signatures.emplace(id,signature);
            }
            bool found=false;
            for(std::size_t j=0;j<metadata.size();++j) if(metadata[j].sha256==sha)
                for(const auto& span:metadata[j].executable_spans)
                    if(address>=span.first && address<span.second && size<=span.second-address) {
                        DWORD old=0;
                        if(!VirtualProtect(images[j].get()+address,static_cast<SIZE_T>(size),PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),images[j].get()+address,static_cast<SIZE_T>(size))) throw std::runtime_error("Original callee permission failed");
                        found=true;
                    }
            if(!found) throw std::runtime_error("Original callee image/code extent missing");
            calls.emplace(id,OriginalCallDeclaration{eboot,sha,address});
        }
    }
    // Session owns this address. Do not retain it beyond the session lifetime.
    // Only previously declared and extent-validated original code is exposed.
    void* resolve_address(const std::string& id) {
        const auto found=calls.find(id);
        if(found==calls.end()) throw std::runtime_error("Original callee id missing");
        const auto& declaration=found->second;
        return resolve_original_call(images,metadata,declaration.eboot_sha256,
                                     declaration.image_sha256,declaration.address);
    }
    OriginalRegisterResult call_registers(const std::string& id,const OriginalRegisterArguments& args) {
        const auto found=calls.find(id);const auto sig=signatures.find(id);
        if(found==calls.end() || sig==signatures.end()) throw std::runtime_error("Typed original callee declaration missing");
        const auto& signature=sig->second;
        validate_register_arguments(args,signature.integers,static_cast<unsigned>(signature.xmm.size()));
        for(unsigned i=0;i<args.xmm_count;++i) if((args.xmm_kinds[i]==XmmArgumentKind::Float32?'f':'d')!=signature.xmm[i]) throw std::runtime_error("Original XMM type differs from declaration");
        if((args.return_kind==OriginalReturnKind::Integer?'i':args.return_kind==OriginalReturnKind::Float32?'f':'d')!=signature.result) throw std::runtime_error("Original return type differs from declaration");
        const auto& declaration=found->second;
        auto* target=resolve_original_call(images,metadata,declaration.eboot_sha256,declaration.image_sha256,declaration.address);
        if(session_bank_call) throw std::runtime_error("Nested original session unsupported");
        BankCall context;context.functions=&functions;context.data=&data;
        auto handler=AddVectoredExceptionHandler(1,session_missing_handler);
        if(!handler) throw std::runtime_error("Original session handler unavailable");
        session_bank_call=&context;
        const auto result=invoke_original_registers(target,args,signature.integers,static_cast<unsigned>(signature.xmm.size()));
        session_bank_call=nullptr;RemoveVectoredExceptionHandler(handler);return result;
    }
    std::uint64_t call(const std::string& id,std::uint64_t first=0,std::uint64_t second=0) {
        if(signatures.count(id)) throw std::runtime_error("Typed original callee requires call_registers");
        const auto found=calls.find(id);
        if(found==calls.end()) throw std::runtime_error("Original callee id missing");
        const auto& declaration=found->second;
        auto* target=resolve_original_call(images,metadata,declaration.eboot_sha256,
                                           declaration.image_sha256,declaration.address);
        return invoke_session_target(target,first,second,functions,data);
    }
};
}
