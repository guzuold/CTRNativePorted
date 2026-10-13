// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "service_dll.hpp"
#include "original_call.hpp"
#include "service_import_record.hpp"
#include "service_record_registry.hpp"
#include "service_adapter_call.hpp"
#include <cstdio>
#include <array>
#include <algorithm>
#include <atomic>
namespace ctrport {
// Synthetic fixed resident record; retail thunks are not installed.
inline const ServiceImportRecord integer_dispatch_record{
    std::string(64,'a'),std::string(64,'a'),"synthetic","test","test",8,1};
inline ServiceGenerations integer_dispatch_generations;
inline ServiceRecordRegistry integer_dispatch_records;
extern "C" void ctr_sysv_waiting_integer3();
extern "C" std::uint64_t ctr_waiting_integer3(std::uint64_t a,std::uint64_t b,
    std::uint64_t c,std::uint64_t caller){
    try{
        const auto& record=integer_dispatch_record;
        validate_service_import_record(record,std::string(64,'a'),std::string(64,'a'),16);
        const auto held=integer_dispatch_generations.wait_for(record.nid,record.library,record.module);
        const auto target=resolve_service_import(record,*held);
        return call_service_integer_adapter(target,record.signature_id,a,b,c);
    }catch(const std::exception& error){
        std::fprintf(stderr,"NID=synthetic library=test caller=0x%llx dispatch failure: %s\n",
            static_cast<unsigned long long>(caller),error.what());ExitProcess(4);
    }
}
extern "C" void ctr_sysv_record_integer3();
extern "C" std::uint64_t ctr_waiting_record_integer3(std::uint64_t token,
    std::uint64_t a,std::uint64_t b,std::uint64_t c,std::uint64_t caller){
    std::shared_ptr<const ServiceImportRecord> record;
    try{
        record=integer_dispatch_records.acquire(token);
        const auto held=integer_dispatch_generations.wait_for(record->nid,record->library,record->module);
        const auto target=resolve_service_import(*record,*held);
        return call_service_integer_adapter(target,record->signature_id,a,b,c);
    }catch(const std::exception& error){
        std::fprintf(stderr,"Resident service token=%llu NID=%s library=%s caller=0x%llx failure: %s\n",
            static_cast<unsigned long long>(token),record?record->nid.c_str():"<unregistered>",
            record?record->library.c_str():"<unregistered>",static_cast<unsigned long long>(caller),error.what());ExitProcess(4);
    }
}
inline ServiceGenerations integer9_dispatch_generations;
extern "C" void ctr_sysv_record_integer9();
extern "C" std::uint64_t ctr_waiting_record_integer9(std::uint64_t token,const std::uint64_t* arguments,std::uint64_t caller){
    std::shared_ptr<const ServiceImportRecord> record;
    try {
        record=integer_dispatch_records.acquire(token);
        if(record->signature_id!=9 || !arguments)throw std::runtime_error("Nine-argument service identity invalid");
        std::array<std::uint64_t,9> owned{};std::copy_n(arguments,9,owned.begin());
        std::fprintf(stderr,"PENDING service9 resident NID=%s library=%s caller=0x%llx owned_arguments=9\n",record->nid.c_str(),record->library.c_str(),static_cast<unsigned long long>(caller));std::fflush(stderr);
        const auto held=integer9_dispatch_generations.wait_for(record->nid,record->library,record->module);
        const auto target=resolve_service_import(*record,*held);
        CtrInteger9Transport call{};std::memcpy(&call,&target,sizeof(call));
        std::uint64_t result=0;
        if(!call || call(owned.data(),caller,&result))throw std::runtime_error("Nine-argument service transport failed");
        return result;
    }catch(const std::exception& error){
        std::fprintf(stderr,"service9 NID=%s library=%s caller=0x%llx: %s\n",record?record->nid.c_str():"<unregistered>",record?record->library.c_str():"<unregistered>",static_cast<unsigned long long>(caller),error.what());ExitProcess(4);
    }
}
class ServiceIntegerThunk {
    void* memory=nullptr;
public:
    explicit ServiceIntegerThunk(std::uint64_t token,std::uint32_t signature=1){
        if(signature!=1 && signature!=2 && signature!=9)throw std::runtime_error("Thunk signature unsupported");
        // mov r11,record; mov rax,resident bridge; jmp rax. No RWX interval.
        unsigned char bytes[22]={0x49,0xbb,0,0,0,0,0,0,0,0,0x48,0xb8,0,0,0,0,0,0,0,0,0xff,0xe0};
        const auto pointer=token;
        const auto bridge=signature==9?reinterpret_cast<const void*>(&ctr_sysv_record_integer9):reinterpret_cast<const void*>(&ctr_sysv_record_integer3);
        std::memcpy(bytes+2,&pointer,8);std::memcpy(bytes+12,&bridge,8);
        memory=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
        if(!memory)throw std::runtime_error("Resident thunk allocation failed");
        std::memcpy(memory,bytes,sizeof(bytes));DWORD previous=0;
        if(!VirtualProtect(memory,4096,PAGE_EXECUTE_READ,&previous) ||
           !FlushInstructionCache(GetCurrentProcess(),memory,sizeof(bytes))){
            VirtualFree(memory,0,MEM_RELEASE);memory=nullptr;
            throw std::runtime_error("Resident thunk executable publication failed");
        }
    }
    ServiceIntegerThunk(const ServiceIntegerThunk&)=delete;
    ServiceIntegerThunk& operator=(const ServiceIntegerThunk&)=delete;
    ~ServiceIntegerThunk(){if(memory)VirtualFree(memory,0,MEM_RELEASE);}
    void* target()const{return memory;}
};
inline int service_stale_thunk_control(){
    const auto token=integer_dispatch_records.add(integer_dispatch_record,std::string(64,'a'),std::string(64,'a'),16);
    ServiceIntegerThunk thunk(token);integer_dispatch_records.retire(token);
    (void)ctr_call_original_integer3(thunk.target(),1,2,3);
    throw std::runtime_error("Stale record thunk returned");
}
inline int service_record_thunk_control(const std::filesystem::path& root){
    const auto token=integer_dispatch_records.add(integer_dispatch_record,std::string(64,'a'),std::string(64,'a'),16);
    ServiceIntegerThunk thunk(token);
    MEMORY_BASIC_INFORMATION info{};
    if(!VirtualQuery(thunk.target(),&info,sizeof(info)) || info.Protect!=PAGE_EXECUTE_READ)
        throw std::runtime_error("Resident thunk not RX");
    auto pending=std::async(std::launch::async,[&]{return ctr_call_original_integer3(thunk.target(),10,20,30);});
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(integer_dispatch_generations.waiting_count()!=1){
        if(std::chrono::steady_clock::now()>end)ExitProcess(5);
        std::this_thread::yield();
    }
    CtrHostApi host{};host.size=sizeof(host);host.version=1;
    load_service_generation(integer_dispatch_generations,root,root/L"service_control_1.dll",host,std::string(64,'a'),1);
    if(pending.get()!=61)throw std::runtime_error("Record thunk argument result differs");
    load_service_generation(integer_dispatch_generations,root,root/L"service_control_2.dll",host,std::string(64,'a'),2);
    if(ctr_call_original_integer3(thunk.target(),10,20,30)!=62)
        throw std::runtime_error("Record thunk reload result differs");
    integer_dispatch_generations.shutdown();integer_dispatch_records.retire(token);return 0;
}
inline std::uint64_t service_wrong_version_target(std::uint64_t,std::uint64_t,std::uint64_t){
    // If admission fails to reject, this distinct exit identifies forbidden dispatch.
    ExitProcess(6);
}
inline int service_integer_version_control(){
    integer_dispatch_generations.publish(std::make_shared<int>(1),
        {{"synthetic","test","test",reinterpret_cast<const void*>(&service_wrong_version_target),1}},
        1,std::string(64,'b'));
    (void)ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_waiting_integer3),1,2,3);
    throw std::runtime_error("Wrong version service returned");
}
inline int service_integer_shutdown_control(){
    auto pending=std::async(std::launch::async,[]{
        return ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_waiting_integer3),1,2,3);
    });
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(integer_dispatch_generations.waiting_count()!=1){
        if(std::chrono::steady_clock::now()>end)ExitProcess(5);
        std::this_thread::yield();
    }
    integer_dispatch_generations.shutdown();
    (void)pending.get();
    throw std::runtime_error("Cancelled bridge returned fabricated result");
}
inline int service_integer_dispatch_control(const std::filesystem::path& root){
    auto pending=std::async(std::launch::async,[]{
        return ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_waiting_integer3),
            0x1234567800000000ULL,0x2345678900000000ULL,0x3456789a00000000ULL);
    });
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(integer_dispatch_generations.waiting_count()!=1){
        if(std::chrono::steady_clock::now()>end)ExitProcess(5);
        std::this_thread::yield();
    }
    CtrHostApi host{};host.size=sizeof(host);host.version=1;
    load_service_generation(integer_dispatch_generations,root,root/L"service_control_1.dll",host,std::string(64,'a'),1);
    if(pending.get()!=0x69d0369b00000001ULL)throw std::runtime_error("Waiting SysV integer arguments differ");
    load_service_generation(integer_dispatch_generations,root,root/L"service_control_2.dll",host,std::string(64,'a'),2);
    if(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_waiting_integer3),10,20,30)!=62)
        throw std::runtime_error("Reloaded SysV integer dispatch differs");
    integer_dispatch_generations.shutdown();return 0;
}
}
