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

}
