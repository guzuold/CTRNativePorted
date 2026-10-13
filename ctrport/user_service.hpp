// SPDX-License-Identifier: GPL-2.0-only
// Independent local service lifecycle. Public null-init usage:
// Scene-Collective/ps4-payload-sdk libPS4/source/sysutil.c getUserIDList.
// Error values from pinned shadPS4 userservice_error.h (GPL-2.0-or-later),
// revision e3ce810f3a653f43ac64ebab63023de281a4103a. No upstream dummy copied.
#pragma once
#include <mutex>
#include <memory>
#include <stdexcept>
#include <array>
#include "kernel_time.hpp"
namespace ctrport {
class UserService {
    struct Session {int local_user_id;bool login_pending=true;}; // Runtime-owned offline identity, no external account.
    int next_local_user_id=1;
    std::mutex lock;
    std::unique_ptr<Session> session;
public:
    int initialize(const void* params) {
        if(params) throw std::runtime_error("User service priority parameters unsupported");
        std::lock_guard<std::mutex> guard(lock);
        if(session) return static_cast<int>(0x80960003U);
        if(next_local_user_id==std::numeric_limits<int>::max())throw std::runtime_error("Local user identity space exhausted");
        session=std::make_unique<Session>(Session{next_local_user_id++});return 0;
    }
    int login_user_list(std::int32_t* output) {
        if(!output)return static_cast<int>(0x80960005U);
        require_time_output(output,4*sizeof(*output));
        std::lock_guard<std::mutex> guard(lock);
        if(!session)return static_cast<int>(0x80960002U);
        const std::array<std::int32_t,4> ids{session->local_user_id,-1,-1,-1};
        std::memcpy(output,ids.data(),sizeof(ids));return 0;
    }
    // Public ABI/null behavior: pinned userservice.cpp513; identity comes from
    // this runtime's real initialized offline session, never an invented account.
    int initial_user(std::int32_t* output){
        if(!output)return static_cast<int>(0x80960005U);
        require_time_output(output,sizeof(*output));
        std::lock_guard<std::mutex> guard(lock);
        if(!session)return static_cast<int>(0x80960002U);
        *output=session->local_user_id;return 0;
    }
    int get_event(std::int32_t* output){
        if(!output)return static_cast<int>(0x80960005U);
        require_time_output(output,2*sizeof(*output));
        std::lock_guard<std::mutex> guard(lock);
        if(!session)return static_cast<int>(0x80960002U);
        if(!session->login_pending)return static_cast<int>(0x80960007U);
        const std::array<std::int32_t,2> event{0,session->local_user_id};
        std::memcpy(output,event.data(),sizeof(event));session->login_pending=false;return 0;
    }
    int get_name(int id,char* output,size_t size) {
        if(id==-1 || !output)return static_cast<int>(0x80960005U);
        std::lock_guard<std::mutex> guard(lock);
        if(!session)return static_cast<int>(0x80960002U);
        if(id!=session->local_user_id)return static_cast<int>(0x80960009U);
        constexpr char name[]="CTRPlayer"; // Name of our runtime-owned offline session.
        if(size<sizeof(name))return static_cast<int>(0x8096000aU);
        require_time_output(output,sizeof(name));std::memcpy(output,name,sizeof(name));return 0;
    }
    int terminate() {
        std::lock_guard<std::mutex> guard(lock);
        if(!session) return static_cast<int>(0x80960002U);
        session.reset();return 0;
    }
    bool has_user(int id){std::lock_guard<std::mutex> guard(lock);return session && session->local_user_id==id;}
    bool initialized() {std::lock_guard<std::mutex> guard(lock);return bool(session);}
};
inline UserService user_service;
// ABI, null/short-buffer behavior: pinned shadPS4 userservice.cpp1098-1131,
// revision e3ce810f3a653f43ac64ebab63023de281a4103a; independent owned-session implementation.
extern "C" int ctr_user_name(int id,char* output,size_t size,std::uint64_t caller) {
 try{return user_service.get_name(id,output,size);}catch(const std::exception& e){std::fprintf(stderr,"NID=1xxcMiGu2fo library=libSceUserService caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_user_name();
// Offline account has no PSN identity: error without writing a fabricated ID.
// Public ABI/errors: pinned np_manager.cpp594-615 and np_error.h, GPL-2.0-or-later.
inline int offline_np_id(int id,void* output){
 if(id==-1 || !output)return static_cast<int>(0x80550003U);
 if(!user_service.has_user(id))return static_cast<int>(0x80550007U);
 return static_cast<int>(0x80550006U);
}
extern "C" int ctr_np_id(int id,void* output,std::uint64_t caller){
 const int result=offline_np_id(id,output);
 std::printf("{\"scope\":\"original_NpGetNpId_return\",\"caller\":%llu,\"result\":%d,\"output_written\":false}\n",static_cast<unsigned long long>(caller),result);std::fflush(stdout);return result;
}
extern "C" void ctr_sysv_np_id();
inline int user_name_control(){
 UserService service;char output[12];std::memset(output,0x5a,sizeof(output));
 if(service.get_name(1,output+1,10)!=static_cast<int>(0x80960002U) || service.initialize(nullptr))return 1;
 if(service.get_name(2,output+1,10)!=static_cast<int>(0x80960009U) || service.get_name(1,output+1,9)!=static_cast<int>(0x8096000aU) || output[1]!=0x5a)return 2;
 if(service.get_name(1,output+1,10) || std::strcmp(output+1,"CTRPlayer") || output[0]!=0x5a || output[11]!=0x5a)return 3;
 if(user_service.initialize(nullptr))return 4;
 if(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_user_name),1,reinterpret_cast<std::uint64_t>(output+1),10))return 5;
 if(static_cast<int>(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_np_id),1,reinterpret_cast<std::uint64_t>(output),0))!=static_cast<int>(0x80550006U) || output[0]!=0x5a)return 6;
 if(offline_np_id(-1,output)!=static_cast<int>(0x80550003U) || offline_np_id(2,output)!=static_cast<int>(0x80550007U))return 7;
 std::cout<<"{\"scope\":\"owned_user_name\",\"passed\":true}\n";return 0;
}
inline int user_login_list_control() {
    UserService service;std::array<std::int32_t,6> output{71,72,73,74,75,76};
    if(service.login_user_list(output.data()+1)!=static_cast<int>(0x80960002U) || output[1]!=72)return 1;
    if(service.login_user_list(nullptr)!=static_cast<int>(0x80960005U) || service.initialize(nullptr) || service.login_user_list(output.data()+1))return 1;
    if(output!=std::array<std::int32_t,6>{71,1,-1,-1,-1,76})return 1;
    if(service.terminate() || service.login_user_list(output.data()+1)!=static_cast<int>(0x80960002U) || output[1]!=1)return 1;
    if(service.initialize(nullptr) || service.login_user_list(output.data()+1) || output[1]!=2 || output[0]!=71 || output[5]!=76)return 1;
    auto* page=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READONLY);if(!page)throw std::runtime_error("User list control allocation failed");
    bool rejected=false;try{service.login_user_list(static_cast<std::int32_t*>(page));}catch(const std::runtime_error&){rejected=true;}
    VirtualFree(page,0,MEM_RELEASE);if(!rejected)return 1;
    std::cout<<"{\"scope\":\"local_user_login_list\",\"four_int32_slots\":true,\"buffer_canaries_preserved\":true,\"session_identity_changes_after_terminate\":true,\"null_readonly_uninitialized_rejected\":true,\"external_accounts_used\":false}\n";return 0;
}
extern "C" int ctr_user_login_list(std::int32_t* output,std::uint64_t caller) {
    try{return user_service.login_user_list(output);}catch(const std::exception& error){std::fprintf(stderr,"NID=fPhymKNvK-A library=libSceUserService caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_user_login_list();
extern "C" int ctr_user_event(std::int32_t* output,uint64_t caller){
 try{const int result=user_service.get_event(output);std::printf("{\"scope\":\"original_UserServiceGetEvent_return\",\"caller\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),result);std::fflush(stdout);return result;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=yH17Q6NWtVg library=libSceUserService caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_user_event();
inline int user_event_control(){
 UserService service;std::array<int32_t,4> result{71,72,73,74};
 if(service.get_event(result.data()+1)!=static_cast<int>(0x80960002U) || result[1]!=72)return 1;
 if(service.initialize(nullptr) || service.get_event(nullptr)!=static_cast<int>(0x80960005U))return 1;
 auto* page=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READONLY);if(!page)return 1;
 bool rejected=false;try{service.get_event(static_cast<int32_t*>(page));}catch(...){rejected=true;}VirtualFree(page,0,MEM_RELEASE);
 if(!rejected || service.get_event(result.data()+1) || result!=std::array<int32_t,4>{71,0,1,74})return 1;
 if(service.get_event(result.data()+1)!=static_cast<int>(0x80960007U) || result!=std::array<int32_t,4>{71,0,1,74})return 1;
 if(service.terminate() || service.initialize(nullptr) || service.get_event(result.data()+1) || result[2]!=2)return 1;
 std::cout<<"{\"scope\":\"owned_user_login_event_queue\",\"passed\":true,\"consume_once\":true,\"failed_write_preserves_event\":true,\"external_accounts\":false}\n";return 0;
}
extern "C" int ctr_user_initial(std::int32_t* output,uint64_t caller){
 try{const int result=user_service.initial_user(output);std::printf("{\"scope\":\"original_UserServiceGetInitialUser_return\",\"caller\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),result);std::fflush(stdout);return result;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=CdWp0oHWGr0 library=libSceUserService caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_user_initial();
inline int user_initial_control(){
 UserService service;std::array<int32_t,3> output{71,72,73};
 if(service.initial_user(output.data()+1)!=static_cast<int>(0x80960002U) || output[1]!=72)return 1;
 if(service.initial_user(nullptr)!=static_cast<int>(0x80960005U) || service.initialize(nullptr) || service.initial_user(output.data()+1) || output!=std::array<int32_t,3>{71,1,73})return 2;
 std::array<int32_t,2> event{};if(service.get_event(event.data()) || service.initial_user(output.data()+1) || output[1]!=event[1])return 3;
 if(service.terminate() || service.initialize(nullptr) || service.initial_user(output.data()+1) || output[1]!=2)return 4;
 auto* page=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READONLY);if(!page)return 5;
 bool rejected=false;try{service.initial_user(static_cast<int32_t*>(page));}catch(...){rejected=true;}VirtualFree(page,0,MEM_RELEASE);if(!rejected)return 6;
 if(user_service.initialize(nullptr) || ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_user_initial),reinterpret_cast<uint64_t>(output.data()+1),0) || output!=std::array<int32_t,3>{71,1,73})return 7;
 std::cout<<"{\"scope\":\"owned_initial_user_control\",\"passed\":true,\"session_identity\":true,\"canaries\":true,\"readonly_rejected\":true,\"event_does_not_change_initial_identity\":true}\n";return 0;
}

// Retail NpToolkit2 SHA480a769d...e5c9c: call0x41702 passes function in
// RDI and context in RSI; callback0x41470 consumes an event pointer.
// Registration only; no fabricated login events or arbitrary callback dispatch.
inline std::mutex user_callback_lock;
inline void* user_callback_function=nullptr;
inline void* user_callback_argument=nullptr;
inline int user_callback_register_owned(void* function,void* argument){
    if(!function)throw std::runtime_error("Null user callback contract not characterized");
    std::lock_guard<std::mutex> lock(user_callback_lock);
    if(user_callback_function)throw std::runtime_error("Multiple user callback registration not characterized");
    user_callback_function=function;user_callback_argument=argument;return 0;
}
inline int user_callback_registration_control(){
    int function=1,argument=2;bool null_rejected=false,duplicate_rejected=false;
    try{user_callback_register_owned(nullptr,&argument);}catch(const std::runtime_error&){null_rejected=true;}
    if(!null_rejected || user_callback_function || user_callback_register_owned(&function,&argument) || user_callback_function!=&function || user_callback_argument!=&argument)return 1;
    try{user_callback_register_owned(&argument,nullptr);}catch(const std::runtime_error&){duplicate_rejected=true;}
    if(!duplicate_rejected || user_callback_function!=&function || user_callback_argument!=&argument)return 1;
    user_callback_function=nullptr;user_callback_argument=nullptr;
    std::cout<<"{\"scope\":\"user_callback_registration_control\",\"retained_function_and_context\":true,\"null_and_duplicate_atomic\":true,\"callback_executed\":false}\n";return 0;
}

}
