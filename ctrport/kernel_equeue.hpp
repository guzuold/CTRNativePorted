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
inline int kernel_equeue_boundary_control(){
    const auto error=[](unsigned value){return static_cast<int>(value);};
    std::uint64_t token=0;DisplayKernelEvent event{};int count=-1;std::uint32_t zero=0;
    if(kernel_equeue_create(&token,"synthetic") || !token)throw std::runtime_error("Guest equeue create failed");
    if(kernel_equeue_wait(token,&event,1,&count,&zero)!=error(0x8002003cU) || count)
        throw std::runtime_error("Guest equeue timeout differs");
    auto api=display_event_api(kernel_display_events);
    if(api.add(api.context,token,6,0x1234) || api.trigger(api.context,token,6,6) ||
       kernel_equeue_wait(token,&event,1,&count,&zero) || count!=1 || event.udata!=0x1234)
        throw std::runtime_error("Guest equeue output event differs");
    unsigned checks=0;
    if(kernel_equeue_create(nullptr,"name")==error(0x80020016U))++checks;
    if(kernel_equeue_create(&token,nullptr)==error(0x80020016U))++checks;
    if(kernel_equeue_create(reinterpret_cast<std::uint64_t*>(1),"name")==error(0x8002000eU))++checks;
    if(kernel_equeue_create(&token,reinterpret_cast<const char*>(1))==error(0x8002000eU))++checks;
    const char too_long[]="123456789012345678901234567890123";
    if(kernel_equeue_create(&token,too_long)==error(0x8002003fU))++checks;
    if(kernel_equeue_wait(token,nullptr,1,&count,&zero)==error(0x8002000eU))++checks;
    if(kernel_equeue_wait(token,&event,0,&count,&zero)==error(0x80020016U) && !count)++checks;
    if(kernel_equeue_wait(token,&event,1,&count,reinterpret_cast<const std::uint32_t*>(1))==error(0x8002000eU))++checks;
    if(kernel_equeue_delete(token) || kernel_equeue_delete(token)!=error(0x80020009U) ||
       kernel_equeue_wait(token,&event,1,&count,&zero)!=error(0x80020009U) || checks!=8)
        throw std::runtime_error("Guest equeue negative controls differ");
    return 0;
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
inline int gnm_graphics_equeue_control(){
 uint64_t queue=0;if(kernel_equeue_create(&queue,"graphics-control"))return 1;
 auto& queues=kernel_display_events.queues;queues.add(queue,6,0x1111);
 if(kernel_gnm_add_eq_event(queue,6,0x2222) || kernel_gnm_add_eq_event(queue,64,0x3333))return 2;
 queues.trigger(queue,6,6,0x1234);queues.graphics_trigger(queue,6);
 auto rows=queues.poll(queue,4);if(rows.size()!=2)return 3;
 bool display=false,graphics=false;
 for(const auto& row:rows){if(row.filter==-13)display=row.udata==0x1111 && row.flags==0x21;
  if(row.filter==-14){int type=-1;graphics=row.ident==6 && row.data==6 && row.udata==0x2222 && row.flags==0x20 && !kernel_gnm_get_eq_event_type(&row,type) && type==6;}}
 if(!display || !graphics || !queues.poll(queue,4).empty())return 4;
 if(kernel_gnm_delete_eq_event(queue,6) || kernel_gnm_delete_eq_event(queue,6))return 5;
 queues.trigger(queue,6,6,0x1234);rows=queues.poll(queue,4);if(rows.size()!=1 || rows[0].filter!=-13)return 6;
 bool removed=false;try{queues.graphics_trigger(queue,6);}catch(const std::runtime_error&){removed=true;}if(!removed)return 7;
 auto waiter=std::async(std::launch::async,[&](){return queues.wait(queue,1,nullptr);});
 const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(2);
 while(!queues.waiting_count(queue) && std::chrono::steady_clock::now()<limit)std::this_thread::yield();
 if(!queues.waiting_count(queue)){queues.destroy(queue);waiter.wait();return 8;}
 queues.graphics_signal(64);rows=waiter.get();if(rows.size()!=1 || rows[0].filter!=-14 || rows[0].data!=64 || rows[0].udata!=0x3333)return 9;
 uint64_t other=0;if(kernel_equeue_create(&other,"graphics-fanout") || kernel_gnm_add_eq_event(other,64,0x7788))return 12;
 queues.graphics_signal(64);auto second=queues.poll(other,1);rows=queues.poll(queue,1);
 if(second.size()!=1 || rows.size()!=1 || second[0].udata!=0x7788 || rows[0].udata!=0x3333)return 13;
 if(kernel_gnm_delete_eq_event(other,64))return 14;queues.graphics_signal(64);
 if(!queues.poll(other,1).empty() || queues.poll(queue,1).size()!=1 || kernel_equeue_delete(other))return 15;
 if(kernel_gnm_add_eq_event(queue,7,0)!=88)return 10;
 if(kernel_equeue_delete(queue) || kernel_gnm_add_eq_event(queue,64,0)!=static_cast<int>(0x80020009u) || kernel_gnm_delete_eq_event(queue,64)!=static_cast<int>(0x80020009u))return 11;
 std::cout<<"{\"scope\":\"gnm_graphics_equeue_control\",\"passed\":true,\"filter_identity_coexistence\":true,\"clear_delivery\":true,\"real_blocking_waiter\":true,\"graphics_flags\":32,\"VideoOut_flags_preserved\":33,\"synthetic_IRQ_source\":true,\"runtime_bound\":false}\n";return 0;
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
