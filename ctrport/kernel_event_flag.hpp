// SPDX-License-Identifier: GPL-2.0-only
// Independent implementation. Public contract: shadPS4 event_flag.cpp249-294,
// revision e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later.
#pragma once
#include "settings/hot_trace.hpp"
#include "kernel_time.hpp"
#include <map>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <list>
#include <future>
#include <chrono>
namespace ctrport {
struct KernelEventFlag {
    std::string name;unsigned attribute;std::uint64_t bits;
    std::mutex mutex;
    struct Waiter {std::uint64_t requested,result=0;unsigned mode;bool ready=false;std::condition_variable changed;};
    std::list<Waiter*> waiters;
    KernelEventFlag(std::string n,unsigned a,std::uint64_t b):name(std::move(n)),attribute(a),bits(b){}
    bool matches(const Waiter& w)const{return (w.mode&15)==1?(bits&w.requested)==w.requested:(bits&w.requested)!=0;}
    void consume(Waiter& w){w.result=bits;if((w.mode&0xf0)==0x10)bits=0;else if((w.mode&0xf0)==0x20)bits&=~w.requested;w.ready=true;}
    void set(std::uint64_t value){std::lock_guard<std::mutex> guard(mutex);bits|=value;for(auto* w:waiters)if(!w->ready && matches(*w)){consume(*w);w->changed.notify_one();}}
};
class KernelEventFlags {
    std::mutex lock;
    std::uint64_t next=1;
    std::map<std::uint64_t,std::shared_ptr<KernelEventFlag>> owned;
public:
    int create(std::uint64_t* output,const char* name,unsigned attr,std::uint64_t initial,const void* options) {
        if(!output || !name || options || attr>0x22 || (attr&15)>2 || (attr&0xf0)>0x20)return static_cast<int>(0x80020016U);
        require_time_output(output,8);
        std::string label;
        for(unsigned i=0;i<32;++i){char ch=0;SIZE_T read=0;
            if(!ReadProcessMemory(GetCurrentProcess(),name+i,&ch,1,&read) || read!=1)throw std::runtime_error("Event flag name unreadable");
            if(!ch)break;
            label.push_back(ch);
        }
        if(label.size()==32)return static_cast<int>(0x8002003fU);
        auto object=std::make_shared<KernelEventFlag>(label,attr,initial);
        std::lock_guard<std::mutex> guard(lock);
        if(!next)throw std::runtime_error("Event flag handle space exhausted");
        const auto id=next++;owned.emplace(id,object);*output=id;return 0;
    }
    // Independent FIFO wait/atomic consume, public event_flag.cpp105-185/Wait.
    int wait(std::uint64_t id,std::uint64_t requested,unsigned mode,std::uint64_t* output,std::uint32_t* timeout){
        auto object=find(id);if(!object)return int(0x80020003U);
        if(!requested || (mode&15)<1 || (mode&15)>2 || (mode&~0x3fU) || (mode&0xf0)>0x20)return int(0x80020016U);
        if(output)require_time_output(output,8);if(timeout)require_time_output(timeout,4);
        if((object->attribute&15)==2)throw std::runtime_error("Event flag priority queue not implemented");
        std::unique_lock<std::mutex> guard(object->mutex);
        if((object->attribute&0xf0)!=0x20 && !object->waiters.empty())return int(0x80020001U);
        KernelEventFlag::Waiter waiter;waiter.requested=requested;waiter.mode=mode;
        if(object->matches(waiter)){object->consume(waiter);if(output)*output=waiter.result;return 0;}
        object->waiters.push_back(&waiter);
        if(ctrport::settings::hot_trace()){std::fprintf(stderr,"event flag blocking id=%llu bits=%llu mode=%u thread=%lu\n",static_cast<unsigned long long>(id),static_cast<unsigned long long>(requested),mode,GetCurrentThreadId());std::fflush(stderr);}
        bool ready=true;
        if(timeout)ready=waiter.changed.wait_for(guard,std::chrono::microseconds(*timeout),[&]{return waiter.ready;});
        else waiter.changed.wait(guard,[&]{return waiter.ready;});
        object->waiters.remove(&waiter);
        if(!ready){if(output)*output=object->bits;*timeout=0;return int(0x8002003cU);}
        if(output)*output=waiter.result;
        return 0;
    }
    // Public event_flag.cpp Set/sceKernelSetEventFlag; atomic OR and FIFO grants.
    int set(std::uint64_t id,std::uint64_t bits){auto object=find(id);if(!object)return int(0x80020003U);object->set(bits);return 0;}
    std::shared_ptr<KernelEventFlag> find(std::uint64_t id){std::lock_guard<std::mutex> guard(lock);const auto it=owned.find(id);return it==owned.end()?nullptr:it->second;}
};
inline KernelEventFlags kernel_event_flags;
extern "C" int ctr_event_flag_create(std::uint64_t* output,const char* name,unsigned attr,std::uint64_t initial,const void* options,std::uint64_t caller){
    try{return kernel_event_flags.create(output,name,attr,initial,options);}catch(const std::exception& error){std::fprintf(stderr,"NID=BpFoboUJoZU library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_event_flag_create();
extern "C" int ctr_event_flag_wait(std::uint64_t id,std::uint64_t bits,unsigned mode,std::uint64_t* output,std::uint32_t* timeout,std::uint64_t caller){
    try{return kernel_event_flags.wait(id,bits,mode,output,timeout);}catch(const std::exception& error){std::fprintf(stderr,"NID=JTvBflhYazQ library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_event_flag_wait();
extern "C" int ctr_event_flag_set(std::uint64_t id,std::uint64_t bits,std::uint64_t caller){
    try{return kernel_event_flags.set(id,bits);}catch(const std::exception& error){std::fprintf(stderr,"NID=IOnSvHzqu6A library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_event_flag_set();
}
