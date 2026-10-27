// SPDX-License-Identifier: GPL-2.0-only
// Own bounded guest output boundary; public equeue.cpp417..489 contract,
// shadPS4 e3ce810f3a653f43ac64ebab63023de281a4103a GPL-2.0-or-later.
#pragma once
#include "display_event_host.hpp"
namespace ctrport {
inline DisplayEventHost kernel_display_events;
inline bool equeue_output(void* pointer,std::size_t size){
    if(!pointer)return false;
    try{require_time_output(pointer,size);return true;}catch(const std::runtime_error&){return false;}
}
inline bool equeue_store(void* pointer,const void* bytes,std::size_t size){
    SIZE_T written=0;return WriteProcessMemory(GetCurrentProcess(),pointer,bytes,size,&written) && written==size;
}
inline int kernel_equeue_create(std::uint64_t* output,const char* name){
    if(!output || !name)return static_cast<int>(0x80020016U);
    if(!equeue_output(output,sizeof(*output)))return static_cast<int>(0x8002000eU);
    bool terminated=false;
    for(std::size_t i=0;i<=32;++i){char value=0;SIZE_T read=0;
        if(!ReadProcessMemory(GetCurrentProcess(),name+i,&value,1,&read) || read!=1)return static_cast<int>(0x8002000eU);
        if(!value){terminated=true;break;}}
    if(!terminated)return static_cast<int>(0x8002003fU);
    auto token=kernel_display_events.queues.create();
    if(!equeue_store(output,&token,sizeof(token))){kernel_display_events.queues.destroy(token);return static_cast<int>(0x8002000eU);}
    return 0;
}
inline int kernel_equeue_delete(std::uint64_t token){
    try{kernel_display_events.queues.destroy(token);return 0;}
    catch(const std::runtime_error&){return static_cast<int>(0x80020009U);}
}
inline int kernel_equeue_wait(std::uint64_t token,DisplayKernelEvent* events,int capacity,int* count,const std::uint32_t* timeout){
    // Validate identity first, matching upstream EBADF precedence.
    try{kernel_display_events.queues.waiting_count(token);}catch(const std::runtime_error&){return static_cast<int>(0x80020009U);}
    if(!equeue_output(count,sizeof(*count)))return static_cast<int>(0x8002000eU);
    if(capacity<1){const int zero=0;equeue_store(count,&zero,sizeof(zero));return static_cast<int>(0x80020016U);}
    if(capacity>64)throw std::runtime_error("Guest equeue capacity above64 unsupported");
    if(!equeue_output(events,sizeof(*events)*static_cast<std::size_t>(capacity)))return static_cast<int>(0x8002000eU);
    std::uint32_t captured_timeout=0;SIZE_T read=0;
    if(timeout && (!ReadProcessMemory(GetCurrentProcess(),timeout,&captured_timeout,sizeof(captured_timeout),&read) || read!=sizeof(captured_timeout)))
        return static_cast<int>(0x8002000eU);
    std::vector<DisplayKernelEvent> rows;
    try{rows=kernel_display_events.queues.wait(token,static_cast<std::size_t>(capacity),timeout?&captured_timeout:nullptr);}
    catch(const std::runtime_error&){return static_cast<int>(0x80020009U);}
    const auto actual=static_cast<int>(rows.size());
    if((actual && !equeue_store(events,rows.data(),rows.size()*sizeof(*events))) ||
       !equeue_store(count,&actual,sizeof(actual)))throw std::runtime_error("Guest equeue outputs changed during wait");
    return actual?0:static_cast<int>(0x8002003cU);
}
extern "C" int ctr_equeue_create(std::uint64_t* output,const char* name,std::uint64_t caller){
    try{return kernel_equeue_create(output,name);}
    catch(const std::exception& error){std::fprintf(stderr,"NID=D0OdFMjp46I library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" int ctr_equeue_delete(std::uint64_t token,std::uint64_t caller){
    try{return kernel_equeue_delete(token);}
    catch(const std::exception& error){std::fprintf(stderr,"NID=jpFjmgAC5AE library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" int ctr_equeue_wait(std::uint64_t token,DisplayKernelEvent* events,int capacity,int* count,const std::uint32_t* timeout,std::uint64_t caller){
    try{return kernel_equeue_wait(token,events,capacity,count,timeout);}
    catch(const std::exception& error){std::fprintf(stderr,"NID=fzyMKs9kim0 library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_equeue_create();
extern "C" void ctr_sysv_equeue_delete();
extern "C" void ctr_sysv_equeue_wait();
extern "C" int ctr_test_equeue_wait(std::uint64_t,DisplayKernelEvent*,int,int*,const std::uint32_t*);

// Public gnmdriver.cpp AddEqEvent/DeleteEqEvent/GetEqEventType contracts,
// pinned e3ce810f3a653f43ac64ebab63023de281a4103a. Unsupported IRQ ids fail88.
// No runtime NID binding or fabricated completion is implied by host controls.
inline int kernel_gnm_add_eq_event(uint64_t queue,uint64_t id,uint64_t udata){
 try{kernel_display_events.queues.waiting_count(queue);}catch(const std::runtime_error&){return static_cast<int>(0x80020009u);}
 if(!DisplayEventQueues::graphics_ident(id))return 88;
 try{kernel_display_events.queues.graphics_add(queue,id,udata);return 0;}catch(const std::runtime_error&){return static_cast<int>(0x80020009u);}
}
inline int kernel_gnm_delete_eq_event(uint64_t queue,uint64_t id){
 try{kernel_display_events.queues.waiting_count(queue);}catch(const std::runtime_error&){return static_cast<int>(0x80020009u);}
 if(!DisplayEventQueues::graphics_ident(id))return 88;
 try{kernel_display_events.queues.graphics_remove(queue,id);return 0;}catch(const std::runtime_error&){return static_cast<int>(0x80020009u);}
}
inline int kernel_gnm_get_eq_event_type(const DisplayKernelEvent* pointer,int& output){
 if(!pointer)return 60;DisplayKernelEvent event{};SIZE_T read=0;
 if(!ReadProcessMemory(GetCurrentProcess(),pointer,&event,sizeof(event),&read) || read!=sizeof(event))return 60;
 output=static_cast<int>(event.data);return 0;
}

extern "C" int ctr_gnm_add_eq_event(uint64_t queue,uint64_t id,uint64_t udata,uint64_t caller){
 const int status=kernel_gnm_add_eq_event(queue,id,udata);
 if(status==88){std::fprintf(stderr,"NID=b0xyllnVY-I library=libSceGnmDriver caller=0x%llx unsupported IRQ=%llu\n",static_cast<unsigned long long>(caller),static_cast<unsigned long long>(id));ExitProcess(4);}return status;
}
extern "C" int ctr_gnm_delete_eq_event(uint64_t queue,uint64_t id,uint64_t caller){
 const int status=kernel_gnm_delete_eq_event(queue,id);
 if(status==88){std::fprintf(stderr,"NID=PVT+fuoS9gU library=libSceGnmDriver caller=0x%llx unsupported IRQ=%llu\n",static_cast<unsigned long long>(caller),static_cast<unsigned long long>(id));ExitProcess(4);}return status;
}
extern "C" int ctr_gnm_get_eq_event_type(const DisplayKernelEvent* pointer,uint64_t caller){
 int type=0;if(kernel_gnm_get_eq_event_type(pointer,type)){std::fprintf(stderr,"NID=UoYY0DWMC0U library=libSceGnmDriver caller=0x%llx event unreadable\n",static_cast<unsigned long long>(caller));ExitProcess(4);}return type;
}
extern "C" void ctr_sysv_gnm_add_eq_event();
extern "C" void ctr_sysv_gnm_delete_eq_event();
extern "C" void ctr_sysv_gnm_get_eq_event_type();

}
