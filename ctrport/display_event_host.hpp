// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "display_event_api.h"
#include "display_event_queue.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "tsc_clock.hpp"
#include <chrono>
#include <thread>
#include <future>
namespace ctrport {
struct DisplayEventHost {
    DisplayEventQueues queues;
    const std::chrono::steady_clock::time_point epoch=std::chrono::steady_clock::now();
};
inline int display_api_add(void* context,std::uint64_t token,std::uint64_t id,std::uint64_t udata){
    if(!context)return 1;
    try{static_cast<DisplayEventHost*>(context)->queues.add(token,id,udata);return 0;}
    catch(const std::exception&){return 1;}
}
inline int display_api_remove(void* context,std::uint64_t token,std::uint64_t id){
    if(!context)return 1;
    try{static_cast<DisplayEventHost*>(context)->queues.remove(token,id);return 0;}
    catch(const std::exception&){return 1;}
}
inline int display_api_trigger(void* context,std::uint64_t token,std::uint64_t id,std::uint64_t hint){
    if(!context)return 1;
    try{static_cast<DisplayEventHost*>(context)->queues.trigger(token,id,hint,fenced_tsc());return 0;}
    catch(const std::exception&){return 1;}
}
inline int display_api_clock(void* context,std::uint64_t* micros,std::uint64_t* tsc){
    if(!context || !micros || !tsc)return 1;
    const auto& host=*static_cast<DisplayEventHost*>(context);
    *micros=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()-host.epoch).count());*tsc=fenced_tsc();return 0;
}
inline CtrDisplayEventApi display_event_api(DisplayEventHost& host){
    return {sizeof(CtrDisplayEventApi),1,&host,&display_api_add,&display_api_remove,&display_api_trigger,&display_api_clock};
}
inline int display_event_api_control(){
    DisplayEventHost host;auto api=display_event_api(host);auto token=host.queues.create();
    if(api.add(api.context,token,6,0x1234) || api.trigger(api.context,token,6,6))throw std::runtime_error("Display callback failed");
    auto rows=host.queues.poll(token,1);if(rows.size()!=1 || rows[0].udata!=0x1234 || (rows[0].data>>12)!=1)
        throw std::runtime_error("Display callback event differs");
    std::uint64_t first=0,second=0,ta=0,tb=0;
    if(api.clock(api.context,&first,&ta))throw std::runtime_error("Display clock failed");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    if(api.clock(api.context,&second,&tb) || second<=first || tb<=ta || !api.clock(nullptr,&second,&tb))
        throw std::runtime_error("Display monotonic clock differs");
    std::promise<void> started;auto ready=started.get_future();bool observed_deleted=false;
    std::thread producer([&]{
        if(api.trigger(api.context,token,6,6)){started.set_value();return;}
        started.set_value();
        for(unsigned i=0;i<1000000;++i){
            if(api.trigger(api.context,token,6,6)){observed_deleted=true;return;}
            std::this_thread::yield();
        }
    });
    ready.wait();host.queues.destroy(token);producer.join();
    if(!observed_deleted || !api.trigger(api.context,token,6,6) || !api.add(api.context,token,6,0))
        throw std::runtime_error("Concurrent deleted display queue accepted");
    auto other=host.queues.create();
    if(api.add(api.context,other,7,0) || api.remove(api.context,other,7) ||
       !api.trigger(api.context,other,7,7))throw std::runtime_error("Display removal boundary differs");
    return 0;
}
inline int display_event_wait_control(){
    DisplayEventHost host;auto token=host.queues.create();host.queues.add(token,7,0x5678);
    const std::uint32_t zero=0,timeout=2000;
    if(!host.queues.wait(token,1,&zero).empty())throw std::runtime_error("Zero wait unexpectedly triggered");
    const auto begin=std::chrono::steady_clock::now();
    if(!host.queues.wait(token,1,&timeout).empty() ||
       std::chrono::steady_clock::now()-begin<std::chrono::microseconds(1500))
        throw std::runtime_error("Finite wait timeout differs");
    std::vector<DisplayKernelEvent> delivered;std::exception_ptr failed;
    std::thread waiter([&]{try{delivered=host.queues.wait(token,1,nullptr);}catch(...){failed=std::current_exception();}});
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!host.queues.waiting_count(token) && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
    const bool sleeping=host.queues.waiting_count(token)==1;
    host.queues.trigger(token,7,7,0xabc);waiter.join();
    if(!sleeping || failed || delivered.size()!=1 || delivered[0].data!=0x1abc)
        throw std::runtime_error("Blocking event wake differs");
    bool deleted=false;
    std::thread closing_waiter([&]{try{host.queues.wait(token,1,nullptr);}catch(const std::runtime_error&){deleted=true;}});
    const auto closing_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!host.queues.waiting_count(token) && std::chrono::steady_clock::now()<closing_deadline)std::this_thread::yield();
    const bool closing_sleep=host.queues.waiting_count(token)==1;
    host.queues.destroy(token);closing_waiter.join();
    if(!closing_sleep || !deleted)throw std::runtime_error("Deleted queue failed to wake blocked waiter");
    return 0;
}

}
