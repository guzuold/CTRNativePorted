// SPDX-License-Identifier: GPL-2.0-only
// Independent implementation. Public contract: shadPS4 event_flag.cpp249-294,
// revision e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later.
#pragma once
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
        std::fprintf(stderr,"event flag blocking id=%llu bits=%llu mode=%u thread=%lu\n",static_cast<unsigned long long>(id),static_cast<unsigned long long>(requested),mode,GetCurrentThreadId());std::fflush(stderr);
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
inline int event_flag_set_control(){
    KernelEventFlags flags;std::uint64_t id=0;flags.create(&id,"contention",0x21,0,nullptr);auto object=flags.find(id);
    auto parked=[&](std::size_t count){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(1);while(std::chrono::steady_clock::now()<end){{std::lock_guard<std::mutex> guard(object->mutex);if(object->waiters.size()==count)return true;}std::this_thread::yield();}return false;};
    std::uint64_t a=0,b=0;
    auto first=std::async(std::launch::async,[&]{std::uint32_t timeout=2000000;return flags.wait(id,1,0x21,&a,&timeout);});
    if(!parked(1)){first.get();return 1;}
    auto second=std::async(std::launch::async,[&]{std::uint32_t timeout=2000000;return flags.wait(id,1,0x21,&b,&timeout);});
    if(!parked(2)){first.get();second.get();return 1;}
    if(flags.set(id,0) || first.wait_for(std::chrono::milliseconds(0))!=std::future_status::timeout || second.wait_for(std::chrono::milliseconds(0))!=std::future_status::timeout)return 1;
    if(flags.set(id,1) || first.get() || a!=1 || second.wait_for(std::chrono::milliseconds(0))!=std::future_status::timeout)return 1;
    if(flags.set(id,1) || second.get() || b!=1 || object->bits)return 1;
    auto left=std::async(std::launch::async,[&]{std::uint32_t timeout=2000000;return flags.wait(id,2,0x21,&a,&timeout);});
    if(!parked(1)){left.get();return 1;}
    auto right=std::async(std::launch::async,[&]{std::uint32_t timeout=2000000;return flags.wait(id,4,0x21,&b,&timeout);});
    if(!parked(2)){left.get();right.get();return 1;}
    if(flags.set(id,6) || left.get() || right.get() || a!=6 || b!=4 || object->bits)return 1;
    if(flags.set(0,1)!=int(0x80020003U) || flags.set(~std::uint64_t(0),1)!=int(0x80020003U))return 1;
    std::cout<<"{\"scope\":\"owned_event_set_contention\",\"fifo_single_consumer_per_bit\":true,\"zero_does_not_wake\":true,\"distinct_bits_wake_both\":true,\"snapshot_before_clear\":true,\"invalid_handles_rejected\":true}\n";return 0;
}
inline int event_flag_wait_control(){
    KernelEventFlags flags;std::uint64_t id=0,out=0;flags.create(&id,"wait",0x21,5,nullptr);
    if(flags.wait(id,1,0x21,&out,nullptr) || out!=5 || flags.find(id)->bits!=4)return 1;
    if(flags.wait(id,6,0x12,&out,nullptr) || out!=4 || flags.find(id)->bits)return 1;
    std::uint32_t timeout=1000;if(flags.wait(id,2,0x21,&out,&timeout)!=int(0x8002003cU) || timeout || out)return 1;
    auto waiter=std::async(std::launch::async,[&]{return flags.wait(id,2,0x21,&out,nullptr);});
    auto object=flags.find(id);bool parked=false;const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(std::chrono::steady_clock::now()<deadline){{std::lock_guard<std::mutex> guard(object->mutex);parked=!object->waiters.empty();}if(parked)break;std::this_thread::yield();}
    if(!parked){object->set(2);waiter.get();return 1;}
    if(waiter.wait_for(std::chrono::milliseconds(0))!=std::future_status::timeout)return 1;
    object->set(2);if(waiter.get() || out!=2 || object->bits)return 1;
    if(!flags.wait(0,1,1,nullptr,nullptr) || !flags.wait(id,0,1,nullptr,nullptr) || !flags.wait(id,1,3,nullptr,nullptr))return 1;
    std::cout<<"{\"scope\":\"owned_fifo_event_wait\",\"and_or_clear_modes\":true,\"timeout\":true,\"real_block_and_wakeup\":true,\"atomic_consumption\":true}\n";return 0;
}
inline int event_flag_create_control(){
    KernelEventFlags flags;std::uint64_t id=0,second=0;
    if(flags.create(&id,"retail-case",0x21,0,nullptr) || !flags.find(id) || flags.find(id)->bits || flags.find(id)->attribute!=0x21)return 1;
    if(flags.create(&second,"pattern",2,0x8000000000000001ULL,nullptr) || second==id || flags.find(second)->bits!=0x8000000000000001ULL)return 1;
    for(unsigned attr:{3U,0x13U,0x23U,0x30U})if(flags.create(&id,"bad",attr,0,nullptr)!=static_cast<int>(0x80020016U))return 1;
    const auto saved=id;
    if(!flags.create(nullptr,"bad",1,0,nullptr) || !flags.create(&id,nullptr,1,0,nullptr) || !flags.create(&id,"bad",1,0,&id) || flags.create(&id,std::string(32,'x').c_str(),1,0,nullptr)!=static_cast<int>(0x8002003fU) || id!=saved)return 1;
    auto* page=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READONLY);if(!page)throw std::runtime_error("Event flag control allocation failed");
    bool rejected=false;try{flags.create(static_cast<std::uint64_t*>(page),"readonly",1,0,nullptr);}catch(const std::runtime_error&){rejected=true;}
    VirtualFree(page,0,MEM_RELEASE);if(!rejected)return 1;
    std::cout<<"{\"scope\":\"owned_event_flag_creation\",\"initial_bits_preserved\":true,\"distinct_handles\":true,\"invalid_parameters_preserve_output\":true,\"readonly_rejected\":true}\n";return 0;
}
}
