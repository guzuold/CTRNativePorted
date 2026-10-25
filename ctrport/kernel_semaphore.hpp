// SPDX-License-Identifier: GPL-2.0-only
// Owned atomic tokens and FIFO/priority host waits. Public reference: shadPS4
// threads/semaphore.cpp316-329, e3ce810f3a653f43ac64ebab63023de281a4103a
// GPL-2.0-or-later. Own implementation; delete/cancel remain missing.
#pragma once
#include "settings/hot_trace.hpp"
#include "kernel_time.hpp"
#include "thread_identity.hpp"
#include <map>
#include <memory>
#include <condition_variable>
#include <list>
namespace ctrport {
struct KernelSemaphore {
    unsigned attribute=0;int maximum=0,tokens=0;
    struct Waiter {int need;int priority;bool granted=false;std::condition_variable changed;};
    std::mutex operation;
    std::list<Waiter*> waiters;
};
class KernelSemaphores {
    std::mutex lock;
    std::uint32_t next=1;
    std::map<std::uint32_t,std::shared_ptr<KernelSemaphore>> owned;
public:
    int create(std::uint32_t* output,const char* name,unsigned attr,int initial,int maximum,const void* options) {
        if(options) throw std::runtime_error("Semaphore options unsupported");
        if(!output || !name || attr>2 || initial<0 || maximum<=0 || initial>maximum) return static_cast<int>(0x80020016U);
        require_time_output(output,4);
        bool terminated=false;
        for(unsigned i=0;i<32;++i) {char ch=0;SIZE_T read=0;
            if(!ReadProcessMemory(GetCurrentProcess(),name+i,&ch,1,&read) || read!=1) throw std::runtime_error("Semaphore name unreadable");
            if(!ch) {terminated=true;break;}}
        if(!terminated) throw std::runtime_error("Semaphore name exceeds supported32bytes");
        auto object=std::make_shared<KernelSemaphore>();object->attribute=attr;object->maximum=maximum;object->tokens=initial;
        std::lock_guard<std::mutex> guard(lock);
        if(!next) throw std::runtime_error("Semaphore handle space exhausted");
        const auto id=next++;owned.emplace(id,object);*output=id;return 0;
    }
    int wait(std::uint32_t id,int need,std::uint32_t* timeout) {
        auto object=find(id);if(!object) return static_cast<int>(0x80020003U);
        if(need<=0 || need>object->maximum) return static_cast<int>(0x80020016U);
        if(timeout) require_time_output(timeout,4);
        std::unique_lock<std::mutex> guard(object->operation);
        if(object->tokens>=need) {object->tokens-=need;return 0;}
        if(timeout && !*timeout) return static_cast<int>(0x8002003cU);
        if(ctrport::settings::hot_trace()){std::fprintf(stderr,"semaphore blocking id=%u need=%d attr=%u priority=%d thread=%lu\n",id,need,object->attribute,effective_guest_priority(),GetCurrentThreadId());std::fflush(stderr);}
        KernelSemaphore::Waiter waiter{need,effective_guest_priority()};
        auto position=object->waiters.end();
        if(object->attribute!=1){position=object->waiters.begin();while(position!=object->waiters.end() && (*position)->priority<=waiter.priority)++position;}
        object->waiters.insert(position,&waiter);
        const auto remove=[&]{object->waiters.remove(&waiter);};
        try {
            if(!timeout){waiter.changed.wait(guard,[&]{return waiter.granted;});return 0;}
            const auto budget=std::chrono::microseconds(*timeout),start=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch());
            const bool granted=waiter.changed.wait_for(guard,budget,[&]{return waiter.granted;});
            const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())-start;
            *timeout=granted && elapsed<budget?static_cast<std::uint32_t>((budget-elapsed).count()):0;
            if(!granted){remove();return static_cast<int>(0x8002003cU);}return 0;
        }catch(...){remove();throw;}
    }
    int poll(std::uint32_t id,int need) {
        auto object=find(id);if(!object)return static_cast<int>(0x80020003U);
        if(need<=0 || need>object->maximum)return static_cast<int>(0x80020016U);
        std::lock_guard<std::mutex> guard(object->operation);
        if(object->tokens<need)return static_cast<int>(0x80020010U);
        object->tokens-=need;return 0;
    }
    int signal(std::uint32_t id,int count) {
        auto object=find(id);if(!object) return static_cast<int>(0x80020003U);
        if(count<=0) return static_cast<int>(0x80020016U);
        std::lock_guard<std::mutex> guard(object->operation);
        if(count>object->maximum-object->tokens)return static_cast<int>(0x80020016U);
        object->tokens+=count;
        for(auto it=object->waiters.begin();it!=object->waiters.end();) {
            auto* waiter=*it;if(waiter->need>object->tokens){++it;continue;}
            object->tokens-=waiter->need;waiter->granted=true;it=object->waiters.erase(it);waiter->changed.notify_one();
        }
        return 0;
    }
    std::shared_ptr<KernelSemaphore> find(std::uint32_t id) {
        std::lock_guard<std::mutex> guard(lock);const auto at=owned.find(id);return at==owned.end()?nullptr:at->second;
    }
};
inline KernelSemaphores kernel_semaphores;
inline int semaphore_contention_control() {
    KernelSemaphores sem;std::uint32_t id=0;
    if(sem.create(&id,"contended",1,7,7,nullptr))return 1;
    std::promise<void> release;auto start=release.get_future().share();std::vector<std::future<int>> attempts;
    for(unsigned i=0;i<8;++i)attempts.push_back(std::async(std::launch::async,[&]{start.wait();std::uint32_t zero=0;return sem.wait(id,2,&zero);}));
    release.set_value();unsigned consumed=0,timedout=0;
    for(auto& attempt:attempts){const auto result=attempt.get();if(!result)++consumed;else if(result==static_cast<int>(0x8002003cU))++timedout;else return 1;}
    if(consumed!=3 || timedout!=5 || sem.find(id)->tokens!=1)return 1;
    std::promise<void> release_signal;auto signal_start=release_signal.get_future().share();attempts.clear();
    for(unsigned i=0;i<8;++i)attempts.push_back(std::async(std::launch::async,[&]{signal_start.wait();return sem.signal(id,1);}));
    release_signal.set_value();unsigned signaled=0,overflow=0;
    for(auto& attempt:attempts){const auto result=attempt.get();if(!result)++signaled;else if(result==static_cast<int>(0x80020016U))++overflow;else return 1;}
    if(signaled!=6 || overflow!=2 || sem.find(id)->tokens!=7)return 1;
    std::cout<<"{\"scope\":\"owned_semaphore_contention\",\"waiters\":8,\"atomic_two_token_consumers\":3,\"zero_timeout_failures\":5,\"signals\":6,\"overflow_rejections\":2,\"blocking_scheduler_used\":false}\n";return 0;
}
inline int semaphore_blocking_control() {
    KernelSemaphores sem;std::uint32_t id=0;if(sem.create(&id,"blocking",1,0,4,nullptr))return 1;
    auto object=sem.find(id);
    auto wait_registered=[&](std::size_t count){const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);for(;;){{std::lock_guard<std::mutex> guard(object->operation);if(object->waiters.size()==count)return;}if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Semaphore control registration deadline");std::this_thread::yield();}};
    auto first=std::async(std::launch::async,[&]{return sem.wait(id,2,nullptr);});wait_registered(1);
    auto second=std::async(std::launch::async,[&]{return sem.wait(id,1,nullptr);});wait_registered(2);
    if(sem.signal(id,1) || second.get() || first.wait_for(std::chrono::milliseconds(5))!=std::future_status::timeout)return 1;
    if(sem.signal(id,2) || first.get())return 1;
    std::uint32_t timeout=1000;if(sem.wait(id,1,&timeout)!=static_cast<int>(0x8002003cU) || timeout)return 1;
    {std::lock_guard<std::mutex> guard(object->operation);if(!object->waiters.empty() || object->tokens)return 1;}
    if(sem.signal(id,1) || sem.wait(id,1,nullptr))return 1;
    std::uint32_t priority_id=0;if(sem.create(&priority_id,"priority",2,0,2,nullptr))return 1;
    object=sem.find(priority_id);
    auto low=std::async(std::launch::async,[&]{current_guest_priority=700;return sem.wait(priority_id,1,nullptr);});wait_registered(1);
    auto high=std::async(std::launch::async,[&]{current_guest_priority=478;return sem.wait(priority_id,1,nullptr);});wait_registered(2);
    if(sem.signal(priority_id,1) || high.get() || low.wait_for(std::chrono::milliseconds(5))!=std::future_status::timeout)return 1;
    if(sem.signal(priority_id,1) || low.get())return 1;
    std::cout<<"{\"scope\":\"fifo_semaphore_blocking\",\"priority478_before700\":true,\"real_host_wait_wake\":true,\"insufficient_waiter_skipped\":true,\"atomic_multi_token_grant\":true,\"timeout_removes_waiter\":true,\"signal_before_wait_preserved\":true}\n";return 0;
}
inline int semaphore_atomic_control() {
    KernelSemaphores sem;std::uint32_t id=0,zero=0;
    if(sem.create(&id,"atomic",1,3,4,nullptr) || sem.wait(id,2,&zero))return 1;
    if(sem.wait(id,2,&zero)!=static_cast<int>(0x8002003cU) || sem.find(id)->tokens!=1)return 1;
    if(sem.signal(id,4)!=static_cast<int>(0x80020016U) || sem.find(id)->tokens!=1)return 1;
    if(sem.signal(id,3) || sem.wait(id,4,&zero) || sem.find(id)->tokens)return 1;
    std::uint32_t timeout=1000;if(sem.wait(id,1,&timeout)!=static_cast<int>(0x8002003cU) || timeout || sem.find(id)->tokens)return 1;
    std::cout<<"{\"scope\":\"owned_semaphore_atomic_tokens\",\"multi_token_atomic\":true,\"insufficient_no_partial_consume\":true,\"overflow_no_mutation\":true,\"finite_wait_timeout_verified\":true}\n";return 0;
}
}
