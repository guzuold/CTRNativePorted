// SPDX-License-Identifier: GPL-2.0-only
// Own bounded kernel display queue implementation. Public contract:
// shadPS4 e3ce810f3a653f43ac64ebab63023de281a4103a
// src/core/libraries/kernel/equeue.{h,cpp}, GPL-2.0-or-later.
// No upstream file copied; original kernel/scheduler imports remain unbound.
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>
#include <condition_variable>
#include <chrono>
namespace ctrport {
struct DisplayKernelEvent {
    std::uint64_t ident;
    std::int16_t filter;
    std::uint16_t flags;
    std::uint32_t fflags;
    std::uint64_t data;
    std::uint64_t udata;
};
static_assert(sizeof(DisplayKernelEvent)==32,"Display event ABI");
class DisplayEventQueues {
    struct Pending {DisplayKernelEvent event;bool triggered=false;};
    struct Queue {
        std::mutex lock;std::condition_variable changed;
        unsigned waiters=0;bool closed=false;std::map<std::pair<std::int16_t,std::uint64_t>,Pending> events;
    };
    std::mutex registry_lock;std::uint64_t next=1;
    std::map<std::uint64_t,std::shared_ptr<Queue>> registry;
    std::shared_ptr<Queue> find(std::uint64_t token){
        std::lock_guard<std::mutex> guard(registry_lock);
        auto it=registry.find(token);if(it==registry.end())throw std::runtime_error("Display queue token absent/deleted");
        return it->second;
    }
    static void live(const Queue& queue){if(queue.closed)throw std::runtime_error("Display queue deleted");}
    static std::vector<DisplayKernelEvent> collect(Queue& queue,std::size_t capacity){
        std::vector<DisplayKernelEvent> result;
        for(auto& item:queue.events){auto& pending=item.second;
            if(pending.triggered){result.push_back(pending.event);pending.triggered=false;pending.event.data=0;pending.event.fflags=0;
                if(result.size()==capacity)break;}}
        return result;
    }
    static void ident(std::uint64_t value){if(value!=6 && value!=7)throw std::runtime_error("Display event identifier unsupported");}
public:
    std::uint64_t create(){
        std::lock_guard<std::mutex> guard(registry_lock);
        if(!next)throw std::runtime_error("Display queue token exhausted");
        auto token=next++;registry.emplace(token,std::make_shared<Queue>());return token;
    }
    void destroy(std::uint64_t token){
        std::shared_ptr<Queue> queue;
        {std::lock_guard<std::mutex> guard(registry_lock);
         auto it=registry.find(token);if(it==registry.end())throw std::runtime_error("Display queue token absent/deleted");
         queue=it->second;registry.erase(it);}
        {std::lock_guard<std::mutex> guard(queue->lock);queue->closed=true;queue->events.clear();}
        queue->changed.notify_all();
    }
    void add(std::uint64_t token,std::uint64_t id,std::uint64_t udata){
        ident(id);auto queue=find(token);std::lock_guard<std::mutex> guard(queue->lock);live(*queue);
        queue->events[{std::int16_t(-13),id}]={{id,-13,0x21,0,0,udata},false};
    }
    void remove(std::uint64_t token,std::uint64_t id){
        ident(id);auto queue=find(token);std::lock_guard<std::mutex> guard(queue->lock);live(*queue);
        if(!queue->events.erase({std::int16_t(-13),id}))throw std::runtime_error("Display event not registered");
    }
    void trigger(std::uint64_t token,std::uint64_t id,std::uint64_t hint,std::uint64_t tsc){
        ident(id);auto queue=find(token);
        {std::lock_guard<std::mutex> guard(queue->lock);live(*queue);
         auto it=queue->events.find({std::int16_t(-13),id});if(it==queue->events.end())throw std::runtime_error("Display event not registered");
         auto& pending=it->second;pending.triggered=true;
         if(hint && (hint&0xff)==id){
             auto count=(pending.event.data>>12)&15;
             if(count!=15)++count;
             pending.event.data=(tsc&0xfff)|(count<<12)|(hint&0xffffffffffff0000ULL);
         }}
        queue->changed.notify_all();
    }
    // Public Gnm AddEqEvent + EqueueInternal AddEvent normalize ADD to CLEAR.
    // These are real registration/delivery operations, not a GPU IRQ producer.
    static bool graphics_ident(std::uint64_t id){return id<=6 || id==64;}
    void graphics_add(std::uint64_t token,std::uint64_t id,std::uint64_t udata){
        if(!graphics_ident(id))throw std::runtime_error("Graphics interrupt identifier unsupported");
        auto queue=find(token);std::lock_guard<std::mutex> guard(queue->lock);live(*queue);
        queue->events[{std::int16_t(-14),id}]={{id,-14,0x20,0,0,udata},false};
    }
    void graphics_remove(std::uint64_t token,std::uint64_t id){
        if(!graphics_ident(id))throw std::runtime_error("Graphics interrupt identifier unsupported");
        auto queue=find(token);std::lock_guard<std::mutex> guard(queue->lock);live(*queue);
        // Public GnmDeleteEqEvent ignores RemoveEvent's not-found result.
        queue->events.erase({std::int16_t(-14),id});
    }
    void graphics_trigger(std::uint64_t token,std::uint64_t id){
        if(!graphics_ident(id))throw std::runtime_error("Graphics interrupt identifier unsupported");
        auto queue=find(token);
        {std::lock_guard<std::mutex> guard(queue->lock);live(*queue);
         auto it=queue->events.find({std::int16_t(-14),id});
         if(it==queue->events.end())throw std::runtime_error("Graphics event not registered");
         it->second.triggered=true;it->second.event.data=id;
        }
        queue->changed.notify_all();
    }
    // Session completion dispatcher: registration lifetime is the retained
    // (queue,filter,ident) event itself; destroyed queues cannot receive IRQs.
    void graphics_signal(std::uint64_t id){
        if(!graphics_ident(id))throw std::runtime_error("Graphics interrupt identifier unsupported");
        std::vector<std::shared_ptr<Queue>> targets;
        {std::lock_guard<std::mutex> guard(registry_lock);for(const auto& item:registry)targets.push_back(item.second);}
        for(const auto& queue:targets){bool found=false;
         {std::lock_guard<std::mutex> guard(queue->lock);if(queue->closed)continue;
          const auto it=queue->events.find({std::int16_t(-14),id});if(it==queue->events.end())continue;
          it->second.triggered=true;it->second.event.data=id;found=true;
         }
         if(found)queue->changed.notify_all();
        }
    }
    std::vector<DisplayKernelEvent> poll(std::uint64_t token,std::size_t capacity){
        if(!capacity || capacity>64)throw std::runtime_error("Display event poll capacity unsupported");
        auto queue=find(token);std::lock_guard<std::mutex> guard(queue->lock);live(*queue);
        return collect(*queue,capacity);
    }
    std::vector<DisplayKernelEvent> wait(std::uint64_t token,std::size_t capacity,const std::uint32_t* timeout_us){
        if(!capacity || capacity>64)throw std::runtime_error("Display event wait capacity unsupported");
        auto queue=find(token);std::unique_lock<std::mutex> guard(queue->lock);live(*queue);
        auto ready=[&]{if(queue->closed)return true;
            for(const auto& item:queue->events)if(item.second.triggered)return true;
            return false;};
        if(!ready() && (!timeout_us || *timeout_us)){
            ++queue->waiters;
            try{
                if(timeout_us)queue->changed.wait_for(guard,std::chrono::microseconds(*timeout_us),ready);
                else queue->changed.wait(guard,ready);
            }catch(...){--queue->waiters;throw;}
            --queue->waiters;
        }
        live(*queue);return collect(*queue,capacity);
    }
    unsigned waiting_count(std::uint64_t token){
        auto queue=find(token);std::lock_guard<std::mutex> guard(queue->lock);live(*queue);return queue->waiters;
    }

};
}

