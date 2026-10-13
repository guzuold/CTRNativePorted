// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "missing_import.hpp"
namespace ctrport {
struct BankCall {
    const MissingTrapBank* functions=nullptr;
    const MissingTrapBank* data=nullptr;
    std::size_t index=0;
    std::uint64_t caller=0;
    bool captured=false,data_access=false;
};
inline int capture_bank_call(EXCEPTION_POINTERS* exception,BankCall* call) noexcept {
    const auto* record=exception->ExceptionRecord;
    const auto rip=exception->ContextRecord->Rip;
    const auto begin=reinterpret_cast<std::uint64_t>(call->functions->memory.get());
    if(record->ExceptionCode==EXCEPTION_ILLEGAL_INSTRUCTION && rip>=begin &&
       rip-begin<call->functions->records.size()*16 && (rip-begin)%16==0) {
        SIZE_T read=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(exception->ContextRecord->Rsp),
                              &call->caller,8,&read) || read!=8) return EXCEPTION_CONTINUE_SEARCH;
        call->index=static_cast<std::size_t>((rip-begin)/16);call->data_access=false;
    } else if(record->ExceptionCode==EXCEPTION_ACCESS_VIOLATION && record->NumberParameters>=2 &&
              record->ExceptionInformation[0]<=1) {
        const auto address=record->ExceptionInformation[1];
        const auto guard=reinterpret_cast<std::uint64_t>(call->data->memory.get());
        if(address<guard || address-guard>=call->data->records.size()*4096) return EXCEPTION_CONTINUE_SEARCH;
        call->index=static_cast<std::size_t>((address-guard)/4096);call->caller=rip;call->data_access=true;
    } else return EXCEPTION_CONTINUE_SEARCH;
    call->captured=true;return EXCEPTION_EXECUTE_HANDLER;
}
inline bool invoke_bank_control(BankCall* call,const unsigned char* address,bool data) {
    __try {
        if(data) { const volatile unsigned char byte=*address; (void)byte; }
        else reinterpret_cast<void(*)()>(const_cast<unsigned char*>(address))();
    } __except(capture_bank_call(GetExceptionInformation(),call)) { return false; }
    return true;
}
inline int check_all_bank_slots(const MissingTrapBank& functions,const MissingTrapBank& data,
                                const std::vector<MappedImage>& images) {
    std::uint64_t checked=0;
    for(bool objects:{false,true}) {
        const auto& bank=objects?data:functions;
        for(std::size_t i=0;i<bank.records.size();++i) {
            const auto& record=bank.records[i];std::uint64_t address=0;
            std::memcpy(&address,images[record.image].get()+record.slot,8);
            BankCall call;call.functions=&functions;call.data=&data;
            if(invoke_bank_control(&call,reinterpret_cast<const unsigned char*>(address),objects) ||
               !call.captured || !call.caller || call.index!=i || call.data_access!=objects)
                throw std::runtime_error("Missing bank dispatcher slot/identity/caller differs");
            ++checked;
        }
    }
    // Synthetic exception records prove that unrelated faults are never swallowed.
    EXCEPTION_RECORD record{};CONTEXT context{};EXCEPTION_POINTERS exception{&record,&context};
    BankCall call;call.functions=&functions;call.data=&data;
    record.ExceptionCode=EXCEPTION_ILLEGAL_INSTRUCTION;context.Rip=1;
    if(capture_bank_call(&exception,&call)!=EXCEPTION_CONTINUE_SEARCH) throw std::runtime_error("Unrelated UD2 swallowed");
    context.Rip=reinterpret_cast<std::uint64_t>(functions.memory.get())+1;
    if(capture_bank_call(&exception,&call)!=EXCEPTION_CONTINUE_SEARCH) throw std::runtime_error("Interior trap RIP swallowed");
    record.ExceptionCode=EXCEPTION_ACCESS_VIOLATION;record.NumberParameters=2;record.ExceptionInformation[0]=0;record.ExceptionInformation[1]=1;
    if(capture_bank_call(&exception,&call)!=EXCEPTION_CONTINUE_SEARCH) throw std::runtime_error("Unrelated data fault swallowed");
    std::cout << "{\"scope\":\"synthetic_host_all_retail_slot_controls\",\"slots_checked\":" << checked
              << ",\"function_traps\":" << functions.records.size() << ",\"data_guards\":" << data.records.size()
              << ",\"unrelated_faults_rejected\":3,\"retail_executed\":false,\"returned_success_from_missing_import\":false}\n";
    return 4;
}
}
