// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "flat_loader.hpp"
namespace ctrport {
struct ObjectBinding { std::string nid,library,module; const void* address; };
struct MissingImportIdentity { std::string nid,library,image_sha256; };
struct MissingImportCall {
    const unsigned char* trap=nullptr;
    std::uint64_t caller=0;
    bool captured=false;
};
// Illegal-instruction traps terminate this call; there is no success return.
inline int capture_missing_import(EXCEPTION_POINTERS* exception,MissingImportCall* call) noexcept {
    if(exception->ExceptionRecord->ExceptionCode!=EXCEPTION_ILLEGAL_INSTRUCTION ||
       exception->ContextRecord->Rip!=reinterpret_cast<std::uint64_t>(call->trap))
        return EXCEPTION_CONTINUE_SEARCH;
    SIZE_T read=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(exception->ContextRecord->Rsp),
                          &call->caller,sizeof(call->caller),&read) || read!=sizeof(call->caller))
        return EXCEPTION_CONTINUE_SEARCH;
    call->captured=true;
    return EXCEPTION_EXECUTE_HANDLER;
}
inline bool invoke_missing_import(MissingImportCall* call) {
    __try { reinterpret_cast<void(*)()>(const_cast<unsigned char*>(call->trap))(); }
    __except(capture_missing_import(GetExceptionInformation(),call)) { return false; }
    return true;
}
struct MissingTrapRecord { std::size_t image; std::uint64_t slot; MissingImportIdentity identity; };
struct MissingTrapBank {
    MappedImage memory;
    std::vector<MissingTrapRecord> records;
};
inline MissingTrapBank install_missing_traps(std::vector<MappedImage>& images,const std::vector<ImageMetadata>& metadata,const std::vector<ObjectBinding>& services={}) {
    if(images.size()!=metadata.size()) throw std::runtime_error("Missing trap image count differs");
    MissingTrapBank bank;
    for(std::size_t i=0;i<metadata.size();++i) for(const auto& symbol:metadata[i].imports) {
        if(symbol.type!=2) continue; // Data objects require their own explicit boundary.
        std::size_t matches=0;
        for(const auto& image:metadata) for(const auto& exported:image.exports)
            if(exported.nid==symbol.nid && exported.library==symbol.library && exported.module==symbol.module && exported.type==symbol.type) ++matches;
        if(matches==1) continue;
        if(symbol.addend) throw std::runtime_error("Missing function trap addend unsupported");
        bool backed=false;
        for(const auto& span:metadata[i].spans)
            if(symbol.slot>=span.first && symbol.slot<=span.second && 8<=span.second-symbol.slot) backed=true;
        if(!backed) throw std::runtime_error("Missing trap slot outside image");
        const ObjectBinding* implemented=nullptr;
        for(const auto& service:services) if(service.nid==symbol.nid && service.library==symbol.library && service.module==symbol.module) {
            if(implemented || !service.address) throw std::runtime_error("Function binding duplicate/null");
            implemented=&service;
        }
        if(implemented) {
            const auto address=reinterpret_cast<std::uint64_t>(implemented->address);
            std::memcpy(images[i].get()+symbol.slot,&address,8);
            std::uint64_t readback=0;std::memcpy(&readback,images[i].get()+symbol.slot,8);
            if(readback!=address) throw std::runtime_error("Function binding readback differs");
            std::cout<<"{\"scope\":\"active_function_binding_readback\",\"image_sha256\":"
                     <<std::quoted(metadata[i].sha256)<<",\"slot\":"<<symbol.slot
                     <<",\"nid\":"<<std::quoted(symbol.nid)<<",\"library\":"
                     <<std::quoted(symbol.library)<<",\"module\":"<<std::quoted(symbol.module)
                     <<",\"pointer_verified\":true,\"full_service_support_claim\":false}\n";
            continue;
        }
        bank.records.push_back({i,symbol.slot,{symbol.nid,symbol.library,metadata[i].sha256}});
    }
    if(bank.records.empty()) return bank;
    const auto size=bank.records.size()*16;
    bank.memory.reset(static_cast<unsigned char*>(VirtualAlloc(nullptr,size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)));
    if(!bank.memory) throw std::runtime_error("Missing trap bank allocation failed");
    std::memset(bank.memory.get(),0xcc,size);
    for(std::size_t i=0;i<bank.records.size();++i) {
        bank.memory.get()[i*16]=0x0f;bank.memory.get()[i*16+1]=0x0b;
    }
    DWORD previous=0;
    if(!VirtualProtect(bank.memory.get(),size,PAGE_EXECUTE_READ,&previous) || !FlushInstructionCache(GetCurrentProcess(),bank.memory.get(),size))
        throw std::runtime_error("Missing trap bank protection failed");
    for(std::size_t i=0;i<bank.records.size();++i) {
        const auto& record=bank.records[i];
        const auto target=reinterpret_cast<std::uint64_t>(bank.memory.get()+i*16);
        std::memcpy(images[record.image].get()+record.slot,&target,8);
        std::uint64_t readback=0;std::memcpy(&readback,images[record.image].get()+record.slot,8);
        if(readback!=target) throw std::runtime_error("Missing trap slot readback differs");
    }
    return bank;
}
inline int check_installed_missing_trap(const MissingTrapBank& bank) {
    if(bank.records.empty()) throw std::runtime_error("Missing trap control requires unresolved function");
    MissingImportCall call;call.trap=bank.memory.get();
    if(invoke_missing_import(&call) || !call.captured || !call.caller)
        throw std::runtime_error("Installed missing trap failed");
    const auto& identity=bank.records.front().identity;
    std::cerr << "CTRPort missing import NID=" << identity.nid << " library=" << identity.library
              << " image=" << identity.image_sha256 << " caller=0x" << std::hex << call.caller << '\n';
    std::cout << "{\"installed_missing_function_traps\":" << bank.records.size()
              << ",\"nid\":" << std::quoted(identity.nid) << ",\"library\":" << std::quoted(identity.library)
              << ",\"caller_address\":" << call.caller
              << ",\"caller_scope\":\"synthetic_host_control\",\"returned_success\":false,\"retail_executed\":false}\n";
    return 4;
}
struct MissingDataCall { const unsigned char* guard=nullptr; std::uint64_t caller=0; bool captured=false; };
inline int capture_missing_data(EXCEPTION_POINTERS* exception,MissingDataCall* call) noexcept {
    const auto* record=exception->ExceptionRecord;
    if(record->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION || record->NumberParameters<2 ||
       record->ExceptionInformation[0]!=0 || record->ExceptionInformation[1]!=reinterpret_cast<ULONG_PTR>(call->guard))
        return EXCEPTION_CONTINUE_SEARCH;
    call->caller=exception->ContextRecord->Rip;call->captured=true;
    return EXCEPTION_EXECUTE_HANDLER;
}
inline bool invoke_missing_data(MissingDataCall* call) {
    __try { const volatile unsigned char byte=*call->guard; (void)byte; }
    __except(capture_missing_data(GetExceptionInformation(),call)) { return false; }
    return true;
}
inline MissingTrapBank install_missing_data_guards(std::vector<MappedImage>& images,const std::vector<ImageMetadata>& metadata,const std::vector<ObjectBinding>& objects={}) {
    if(images.size()!=metadata.size()) throw std::runtime_error("Missing data image count differs");
    MissingTrapBank bank;
    for(std::size_t i=0;i<metadata.size();++i) for(const auto& symbol:metadata[i].imports) {
        if(symbol.type==2) continue;
        if(symbol.type!=1 || symbol.addend) throw std::runtime_error("Missing data guard type/addend unsupported");
        bool backed=false;
        for(const auto& span:metadata[i].spans)
            if(symbol.slot>=span.first && symbol.slot<=span.second && 8<=span.second-symbol.slot) backed=true;
        if(!backed) throw std::runtime_error("Missing data guard slot outside image");
        const ObjectBinding* implemented=nullptr;
        for(const auto& object:objects) if(object.nid==symbol.nid && object.library==symbol.library && object.module==symbol.module) {
            if(implemented || !object.address) throw std::runtime_error("Object binding duplicate/null");
            implemented=&object;
        }
        if(implemented) {
            const auto address=reinterpret_cast<std::uint64_t>(implemented->address);
            std::memcpy(images[i].get()+symbol.slot,&address,8);
            std::uint64_t readback=0;std::memcpy(&readback,images[i].get()+symbol.slot,8);
            if(readback!=address) throw std::runtime_error("Object binding readback differs");
            continue;
        }
        bank.records.push_back({i,symbol.slot,{symbol.nid,symbol.library,metadata[i].sha256}});
    }
    if(bank.records.empty()) return bank;
    bank.memory.reset(static_cast<unsigned char*>(VirtualAlloc(nullptr,bank.records.size()*4096,MEM_RESERVE,PAGE_NOACCESS)));
    if(!bank.memory) throw std::runtime_error("Missing data guard reserve failed");
    for(std::size_t i=0;i<bank.records.size();++i) {
        const auto& record=bank.records[i];const auto address=reinterpret_cast<std::uint64_t>(bank.memory.get()+i*4096);
        std::memcpy(images[record.image].get()+record.slot,&address,8);
        std::uint64_t readback=0;std::memcpy(&readback,images[record.image].get()+record.slot,8);
        if(readback!=address) throw std::runtime_error("Missing data guard slot readback differs");
    }
    return bank;
}
inline int check_installed_missing_data(const MissingTrapBank& bank,const std::vector<MappedImage>& images) {
    if(bank.records.empty()) throw std::runtime_error("Missing data control requires unresolved object");
    const auto& record=bank.records.front();std::uint64_t address=0;
    std::memcpy(&address,images[record.image].get()+record.slot,8);
    MissingDataCall call;call.guard=reinterpret_cast<const unsigned char*>(address);
    if(invoke_missing_data(&call) || !call.captured || !call.caller) throw std::runtime_error("Missing data guard unexpectedly readable");
    std::cerr << "CTRPort missing object NID=" << record.identity.nid << " library=" << record.identity.library
              << " image=" << record.identity.image_sha256 << " caller=0x" << std::hex << call.caller << '\n';
    std::cout << "{\"installed_missing_data_guards\":" << bank.records.size()
              << ",\"nid\":" << std::quoted(record.identity.nid) << ",\"library\":" << std::quoted(record.identity.library)
              << ",\"caller_address\":" << call.caller
              << ",\"caller_scope\":\"synthetic_host_read_via_retail_slot\",\"returned_success\":false,\"retail_executed\":false}\n";
    return 4;
}
inline int missing_import_control() {
    MappedImage trap(static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)));
    if(!trap) throw std::runtime_error("Missing import trap allocation failed");
    trap.get()[0]=0x0f;trap.get()[1]=0x0b; // UD2, deliberately never returns.
    DWORD previous=0;
    if(!VirtualProtect(trap.get(),4096,PAGE_EXECUTE_READ,&previous) ||
       !FlushInstructionCache(GetCurrentProcess(),trap.get(),2))
        throw std::runtime_error("Missing import trap protection failed");
    MissingImportCall call;call.trap=trap.get();
    if(invoke_missing_import(&call) || !call.captured || !call.caller)
        throw std::runtime_error("Missing import trap unexpectedly returned or lost caller");
    const MissingImportIdentity identity{"synthetic_missing_NID","synthetic_library","synthetic_control"};
    std::cerr << "CTRPort missing import NID=" << identity.nid << " library=" << identity.library
              << " image=" << identity.image_sha256 << " caller=0x" << std::hex << call.caller << '\n';
    std::cout << "{\"scope\":\"synthetic_missing_import_call\",\"captured\":true,\"caller_address\":"
              << call.caller << ",\"returned_success\":false,\"retail_executed\":false,\"runtime_replacements\":0}\n";
    return 4;
}
}
