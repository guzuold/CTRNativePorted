// SPDX-License-Identifier: GPL-2.0-only
// New implementation; behavioral reference and license recorded in
// kernel_condition_original_contract.json (pinned shadPS4 condvar.cpp).
#pragma once
#include "kernel_mutex.hpp"
#include "cancellation_control.hpp"
#include <atomic>
#include <set>
namespace ctrport {
inline std::uint64_t condition_observation_base=0;
inline const char* (*condition_observation_image)(std::uint64_t,std::uint64_t&)=nullptr;
inline std::atomic<unsigned> condition_observation_count{0};
inline void observe_condition(const char* phase,std::uint64_t caller,const void* condition,const void* mutex){
    if(!condition_observation_base)return;
    const auto sequence=condition_observation_count.fetch_add(1);if(sequence>=128)return;
    std::uint64_t offset=0;const char* sha=condition_observation_image?condition_observation_image(caller,offset):"unmapped";
    std::fprintf(stderr,"{\"scope\":\"condition_observation\",\"sequence\":%u,\"phase\":\"%s\",\"caller_image_sha256\":\"%s\",\"caller_offset\":%llu,\"condition\":%llu,\"mutex\":%llu,\"thread\":%lu}\n",sequence,phase,sha,static_cast<unsigned long long>(offset),static_cast<unsigned long long>(reinterpret_cast<std::uint64_t>(condition)),static_cast<unsigned long long>(reinterpret_cast<std::uint64_t>(mutex)),GetCurrentThreadId());std::fflush(stderr);
}
// Logarithmic sampling bounds output to 64 events per operation in a run.
inline std::atomic<std::uint64_t> mutex_lock_observations{0},mutex_unlock_observations{0};
inline void observe_mutex_call(bool unlock,std::uint64_t caller,const void* cell){
    if(!condition_observation_image)return;
    const auto count=(unlock?mutex_unlock_observations:mutex_lock_observations).fetch_add(1)+1;
    if(!count || (count&(count-1)))return;
    std::uint64_t offset=0;const char* sha=condition_observation_image(caller,offset);
    std::fprintf(stderr,"{\"scope\":\"mutex_call_sample\",\"operation\":\"%s\",\"count\":%llu,\"caller_image_sha256\":\"%s\",\"caller_offset\":%llu,\"cell\":%llu,\"thread\":%lu}\n",unlock?"unlock":"lock",static_cast<unsigned long long>(count),sha,static_cast<unsigned long long>(offset),static_cast<unsigned long long>(reinterpret_cast<std::uint64_t>(cell)),GetCurrentThreadId());std::fflush(stderr);
}
inline std::atomic<unsigned> condition_frame_observations{0};
inline void observe_condition_frames(std::uint64_t frame,bool waiting=false,const char* scope=nullptr){
    if(!condition_observation_image || (!waiting && condition_frame_observations.fetch_add(1)>=64))return;
    for(unsigned depth=0;depth<8;++depth){
        if(!frame || (frame&7))break;
        std::uint64_t pair[2]{};SIZE_T read=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(frame),pair,sizeof(pair),&read) || read!=sizeof(pair))break;
        std::uint64_t offset=0;const auto sha=condition_observation_image(pair[1],offset);
        std::fprintf(stderr,"{\"scope\":\"%s\",\"thread\":%lu,\"depth\":%u,\"image_sha256\":\"%s\",\"return_offset\":%llu}\n",scope?scope:(waiting?"condition_wait_original_frame":"condition_original_frame"),GetCurrentThreadId(),depth,sha,static_cast<unsigned long long>(offset));
        if(pair[0]<=frame || pair[0]-frame>16*1024*1024)break;frame=pair[0];
    }
    std::fflush(stderr);
}
struct KernelCondition {
    std::string name; // Diagnostic label only; never a shared-object lookup key.
    std::mutex state;
    std::condition_variable changed;
    std::uint64_t generation=0;
    unsigned waiters=0;
};
inline std::mutex condition_registry_lock;
inline std::unordered_map<std::uint64_t,std::shared_ptr<KernelCondition>> condition_registry;
inline std::shared_ptr<KernelCondition> lookup_condition(std::uint64_t* cell) {
    if(!cell) throw std::runtime_error("Condition cell null unsupported");
    require_time_output(cell,8);
    std::lock_guard<std::mutex> guard(condition_registry_lock);
    if(*cell==0) {
        auto object=std::make_shared<KernelCondition>();
        const auto handle=reinterpret_cast<std::uint64_t>(object.get());
        condition_registry.emplace(handle,object);*cell=handle;
    }
    const auto found=condition_registry.find(*cell);
    if(found==condition_registry.end()) throw std::runtime_error("Unknown/destroyed condition handle unsupported");
    return found->second;
}
// Lifecycle reference: pinned condvar.cpp84-95, GPL-2.0-or-later,
// e3ce810f3a653f43ac64ebab63023de281a4103a. Never delete a live waiter.
inline int kernel_condition_destroy(std::uint64_t* cell){
 if(!cell)return static_cast<int>(0x80020016U);require_time_output(cell,8);
 std::lock_guard<std::mutex> guard(condition_registry_lock);
 if(*cell==0)return 0;
 const auto found=condition_registry.find(*cell);
 if(found==condition_registry.end())return static_cast<int>(0x80020016U);
 if(found->second.use_count()!=1)return static_cast<int>(0x80020010U);
 {std::lock_guard<std::mutex> state(found->second->state);if(found->second->waiters)return static_cast<int>(0x80020010U);}
 condition_registry.erase(found);*cell=UINT64_MAX;return 0;
}
extern "C" int ctr_condition_destroy(std::uint64_t* cell,std::uint64_t caller){
 try{const int result=kernel_condition_destroy(cell);std::printf("{\"scope\":\"original_CondDestroy_return\",\"caller\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),result);std::fflush(stdout);return result;}
 catch(const std::exception& e){std::fprintf(stderr,"NID=g+PZd2hiacg library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_condition_destroy();
inline int condition_destroy_control(){
 std::uint64_t cell=0;if(kernel_condition_destroy(&cell) || cell)return 1;
 auto retained=lookup_condition(&cell);const auto handle=cell;
 if(kernel_condition_destroy(&cell)!=static_cast<int>(0x80020010U) || cell!=handle)return 2;
 retained.reset();if(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_condition_destroy),reinterpret_cast<std::uint64_t>(&cell),0,0) || cell!=UINT64_MAX)return 3;
 if(kernel_condition_destroy(&cell)!=static_cast<int>(0x80020016U) || condition_registry.count(handle))return 4;
 std::cout<<"{\"scope\":\"owned_condition_destroy\",\"passed\":true,\"busy_preserved\":true,\"registry_released\":true}\n";return 0;
}
inline int kernel_condition_broadcast(std::uint64_t* cell) {
    auto object=lookup_condition(cell);
    std::lock_guard<std::mutex> guard(object->state);
    ++object->generation;object->changed.notify_all();return 0;
}
// Public condvar.cpp332-336/CondSignal: wake one actual waiter, not all.
inline int kernel_condition_signal(std::uint64_t* cell){
    auto object=lookup_condition(cell);std::lock_guard<std::mutex> guard(object->state);
    ++object->generation;object->changed.notify_one();return 0;
}
extern "C" int ctr_condition_signal(std::uint64_t* cell,std::uint64_t caller){
    try{const int rc=kernel_condition_signal(cell);std::printf("{\"scope\":\"original_CondSignal_return\",\"caller\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),rc);std::fflush(stdout);return rc;}
    catch(const std::exception& e){std::fprintf(stderr,"NID=kDh-NfxgMtE library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_condition_signal();
// Independent infinite host wait; public shadPS4 condvar.cpp CondWait contract,
// revision e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later.
inline int kernel_condition_wait(std::uint64_t* condition,std::uint64_t* mutex,std::uint64_t usec=UINT64_MAX){
    if(usec!=UINT64_MAX && usec>86400000000ULL)throw std::runtime_error("Condition relative timeout above24h unsupported");
    auto cv=lookup_condition(condition);auto mx=lookup_mutex(mutex,false);
    std::unique_lock<std::mutex> guard(cv->state);std::uint32_t depth=0;
    {std::lock_guard<std::mutex> owner(mx->state);if(mx->owner!=std::this_thread::get_id() || !mx->depth)return int(0x80020001U);depth=mx->depth;mx->depth=1;}
    const auto generation=cv->generation;
    if(kernel_mutex_unlock(mutex))throw std::runtime_error("Condition wait release failed");
    ++cv->waiters;std::exception_ptr failure;bool signaled=true;
    std::fprintf(stderr,"condition blocking thread=%lu saved_depth=%u\n",GetCurrentThreadId(),depth);std::fflush(stderr);
    try{if(usec==UINT64_MAX)cv->changed.wait(guard,[&]{return cv->generation!=generation;});else signaled=cv->changed.wait_for(guard,std::chrono::microseconds(usec),[&]{return cv->generation!=generation;});}catch(...){failure=std::current_exception();}
    --cv->waiters;guard.unlock();
    if(kernel_mutex_lock(mutex))throw std::runtime_error("Condition wait reacquire failed");
    {std::lock_guard<std::mutex> owner(mx->state);mx->depth=depth;}
    if(failure)std::rethrow_exception(failure);return signaled?0:int(0x8002003cU);
}
// Own opt-in observer: keep the first original frame chain for each thread/cell.
// No guest writes, wait completion, or verified unwind is inferred.
inline std::mutex condition_wait_frames_lock;
inline std::set<std::pair<DWORD,std::uint64_t>> condition_wait_frames_seen;
inline void observe_wait_frames(std::uint64_t frame,const void* condition){
    char enabled[4]{};if(GetEnvironmentVariableA("CTR_SYNC_TRACE",enabled,sizeof(enabled))!=1 || enabled[0]!='1')return;
    {std::lock_guard<std::mutex> lock(condition_wait_frames_lock);
     if(condition_wait_frames_seen.size()>=256 || !condition_wait_frames_seen.emplace(GetCurrentThreadId(),reinterpret_cast<std::uint64_t>(condition)).second)return;}
    std::fprintf(stderr,"{\"scope\":\"condition_wait_identity\",\"thread\":%lu,\"condition_cell\":%llu,\"guest_modified\":false}\n",GetCurrentThreadId(),static_cast<unsigned long long>(reinterpret_cast<std::uint64_t>(condition)));
    observe_condition_frames(frame,true);
}
extern "C" int ctr_condition_wait_owned(std::uint64_t* condition,std::uint64_t* mutex,std::uint64_t caller,std::uint64_t original_frame){
    try{observe_wait_frames(original_frame,condition);observe_condition("wait_enter",caller,condition,mutex);const auto result=kernel_condition_wait(condition,mutex);observe_condition("wait_return",caller,condition,mutex);return result;}catch(const std::exception& error){std::fprintf(stderr,"NID=WKAXJ4XBPQ4 library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_condition_wait_owned();
inline int condition_wait_owned_control(){
    std::uint64_t condition=0,mutex=0;auto cv=lookup_condition(&condition);auto mx=lookup_mutex(&mutex,true);mx->type=2;
    auto call=[&]{return static_cast<int>(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_condition_wait_owned),reinterpret_cast<std::uint64_t>(&condition),reinterpret_cast<std::uint64_t>(&mutex)));};
    if(call()!=int(0x80020001U))return 1;
    if(kernel_mutex_lock(&mutex) || kernel_mutex_lock(&mutex))return 1;
    auto broadcaster=std::async(std::launch::async,[&]{
        if(kernel_mutex_lock(&mutex))return false;
        bool queued=false;{std::lock_guard<std::mutex> guard(cv->state);queued=cv->waiters==1;}
        const bool result=queued && kernel_condition_broadcast(&condition)==0;
        return kernel_mutex_unlock(&mutex)==0 && result;
    });
    if(call() || !broadcaster.get())return 1;
    {std::lock_guard<std::mutex> guard(mx->state);if(mx->owner!=std::this_thread::get_id() || mx->depth!=2)return 1;}
    if(kernel_mutex_unlock(&mutex) || kernel_mutex_unlock(&mutex))return 1;
    std::cout<<"{\"scope\":\"owned_condition_wait_sysv\",\"mutex_released_for_other_thread\":true,\"real_broadcast_wakeup\":true,\"recursive_depth_restored\":2,\"foreign_owner_eperm\":true}\n";return 0;
}
inline int condition_signal_control(){
    std::uint64_t condition=0,mutex=0;auto cv=lookup_condition(&condition);auto mx=lookup_mutex(&mutex,true);mx->type=2;
    if(kernel_mutex_lock(&mutex) || kernel_mutex_lock(&mutex))return 1;
    auto signaler=std::async(std::launch::async,[&]{
        if(kernel_mutex_lock(&mutex))return false;
        bool queued=false;{std::lock_guard<std::mutex> guard(cv->state);queued=cv->waiters==1;}
        const int rc=static_cast<int>(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_condition_signal),reinterpret_cast<std::uint64_t>(&condition),0));
        return kernel_mutex_unlock(&mutex)==0 && queued && rc==0;
    });
    if(kernel_condition_wait(&condition,&mutex) || !signaler.get())return 1;
    {std::lock_guard<std::mutex> guard(mx->state);if(mx->owner!=std::this_thread::get_id() || mx->depth!=2)return 1;}
    if(kernel_mutex_unlock(&mutex) || kernel_mutex_unlock(&mutex))return 1;
    std::cout<<"{\"scope\":\"condition_signal_control\",\"actual_waiter_woken\":true,\"SysV_signal\":true,\"recursive_depth_restored\":2}\n";return 0;
}
// Public relative-wait contract saturates at the steady clock maximum before
// converting uint64 microseconds to a signed duration. Independent implementation.
inline std::chrono::steady_clock::time_point condition_relative_deadline(std::uint64_t usec) {
    const auto now=std::chrono::steady_clock::now();
    const auto remaining=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::time_point::max()-now).count();
    if(usec>static_cast<std::uint64_t>(remaining))return std::chrono::steady_clock::time_point::max();
    return now+std::chrono::microseconds(static_cast<std::int64_t>(usec));
}
// Isolated bounded host protocol. No guest wait binding or scheduler claim.
inline bool condition_mutex_wait_control(std::uint64_t* condition,std::uint64_t* mutex,std::promise<void>& queued,std::uint64_t timeout_us=2000000) {
    const auto deadline=condition_relative_deadline(timeout_us);
    auto cv=lookup_condition(condition);auto mx=lookup_mutex(mutex,false);
    std::unique_lock<std::mutex> guard(cv->state);
    std::uint32_t saved_depth=0;
    {std::lock_guard<std::mutex> owner(mx->state);
     if(mx->owner!=std::this_thread::get_id() || !mx->depth)throw std::runtime_error("Condition requires owned mutex");
     saved_depth=mx->depth;mx->depth=1;}

    const auto generation=cv->generation;
    if(kernel_mutex_unlock(mutex))throw std::runtime_error("Condition mutex release failed");
    bool woke=false;std::exception_ptr failure;
    try {
        queued.set_value();
        woke=cv->changed.wait_until(guard,deadline,[&]{return cv->generation!=generation;});
    }catch(...) {failure=std::current_exception();}
    guard.unlock();
    if(kernel_mutex_lock(mutex))throw std::runtime_error("Condition mutex reacquire failed");
    {std::lock_guard<std::mutex> owner(mx->state);mx->depth=saved_depth;}
    if(failure)std::rethrow_exception(failure);
    return woke;
}
inline int condition_relative_duration_control() {
    std::uint64_t condition=0,mutex=0;lookup_condition(&condition);
    if(kernel_mutex_lock(&mutex))return 1;
    std::promise<void> queued;auto ready=queued.get_future();
    auto broadcaster=std::async(std::launch::async,[&]{ready.get();
        return kernel_mutex_lock(&mutex)==0 && kernel_condition_broadcast(&condition)==0 && kernel_mutex_unlock(&mutex)==0;});
    const bool woke=condition_mutex_wait_control(&condition,&mutex,queued,~std::uint64_t{0});
    if(!broadcaster.get() || !woke)return 1;
    std::promise<void> timed;const auto start=std::chrono::steady_clock::now();
    if(condition_mutex_wait_control(&condition,&mutex,timed,250))return 1;
    const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    if(elapsed<250 || kernel_mutex_unlock(&mutex))return 1;
    std::cout<<"{\"scope\":\"isolated_condition_relative_duration\",\"uint64_max_did_not_wrap\":true,\"real_broadcast_woke\":true,\"requested_us\":250,\"elapsed_us\":"<<elapsed<<",\"guest_bound\":false}\n";return 0;
}
extern "C" int ctr_condition_relative_missing(std::uint64_t* condition,std::uint64_t* mutex,std::uint64_t usec,std::uint64_t caller) {
    try {
        auto cv=lookup_condition(condition);auto mx=lookup_mutex(mutex,false);(void)cv;
        {std::lock_guard<std::mutex> guard(mx->state);
         if(mx->owner!=std::this_thread::get_id() || !mx->depth)return static_cast<int>(0x80020001U);}
        std::cout<<"{\"scope\":\"condition_relative_missing_arguments\",\"microseconds\":"<<usec<<",\"caller\":\"0x"<<std::hex<<caller<<std::dec<<"\",\"mutex_released\":false}\n"<<std::flush;
        throw std::runtime_error("Guest condition wait scheduler/cancellation missing");
    }catch(const std::exception& error) {
        std::fprintf(stderr,"NID=BmMjYxmew1w library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);
    }
}
extern "C" int ctr_condition_wait_missing(std::uint64_t* condition,std::uint64_t* mutex,std::uint64_t caller) {
    try {
        auto cv=lookup_condition(condition);auto mx=lookup_mutex(mutex,false);(void)cv;
        {std::lock_guard<std::mutex> guard(mx->state);
         if(mx->owner!=std::this_thread::get_id() || !mx->depth)return static_cast<int>(0x80020001U);}
        throw std::runtime_error("Guest condition wait scheduler/cancellation missing");
    }catch(const std::exception& error) {
        std::fprintf(stderr,"NID=WKAXJ4XBPQ4 library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);
    }
}
extern "C" void ctr_sysv_condition_wait_missing();
extern "C" void ctr_condition_wait_fixture();
extern "C" unsigned char ctr_condition_wait_caller;
inline int condition_wait_bridge_missing_control() {
    std::uint64_t condition=0,mutex=0;lookup_condition(&condition);if(kernel_mutex_lock(&mutex))return 1;
    std::cout<<"{\"scope\":\"condition_wait_expected_caller\",\"caller\":\"0x"<<std::hex<<reinterpret_cast<std::uint64_t>(&ctr_condition_wait_caller)<<std::dec<<"\"}\n"<<std::flush;
    ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_condition_wait_fixture),reinterpret_cast<std::uint64_t>(&condition),reinterpret_cast<std::uint64_t>(&mutex));return 1;
}
extern "C" void ctr_sysv_condition_relative_missing();
extern "C" void ctr_condition_relative_fixture();
extern "C" unsigned char ctr_condition_relative_caller;
inline int condition_relative_foreign_owner_control() {
    std::uint64_t condition=0,mutex=0;lookup_condition(&condition);auto mx=lookup_mutex(&mutex,true);mx->type=2;
    if(kernel_mutex_lock(&mutex) || kernel_mutex_lock(&mutex))return 1;
    auto foreign=std::async(std::launch::async,[&]{return static_cast<int>(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_condition_relative_missing),reinterpret_cast<std::uint64_t>(&condition),reinterpret_cast<std::uint64_t>(&mutex),~std::uint64_t{0}));});
    if(foreign.get()!=static_cast<int>(0x80020001U))return 1;
    auto ordinary=std::async(std::launch::async,[&]{return static_cast<int>(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_condition_wait_missing),reinterpret_cast<std::uint64_t>(&condition),reinterpret_cast<std::uint64_t>(&mutex)));});
    if(ordinary.get()!=static_cast<int>(0x80020001U))return 1;
    {std::lock_guard<std::mutex> guard(mx->state);if(mx->owner!=std::this_thread::get_id() || mx->depth!=2)return 1;}
    if(kernel_mutex_unlock(&mutex) || kernel_mutex_unlock(&mutex))return 1;
    std::cout<<"{\"scope\":\"relative_condition_foreign_owner\",\"real_sysv_bridge\":true,\"orbis_eperm\":true,\"owner_depth_unchanged\":true,\"successful_guest_wait\":false}\n";return 0;
}
inline int condition_relative_bridge_missing_control() {
    std::uint64_t condition=0,mutex=0;lookup_condition(&condition);if(kernel_mutex_lock(&mutex))return 1;
    std::cout<<"{\"scope\":\"condition_relative_expected_caller\",\"caller\":\"0x"<<std::hex<<reinterpret_cast<std::uint64_t>(&ctr_condition_relative_caller)<<std::dec<<"\"}\n"<<std::flush;
    ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_condition_relative_fixture),reinterpret_cast<std::uint64_t>(&condition),reinterpret_cast<std::uint64_t>(&mutex),~std::uint64_t{0});
    return 1;
}
inline int condition_failure_reacquire_control() {
    std::uint64_t condition=0,mutex=0;lookup_condition(&condition);
    auto mx=lookup_mutex(&mutex,true);mx->type=2;
    if(kernel_mutex_lock(&mutex) || kernel_mutex_lock(&mutex))return 1;
    // Inject a concrete host publication failure after the mutex was released.
    std::promise<void> already_published;already_published.set_value();bool rejected=false;
    try{condition_mutex_wait_control(&condition,&mutex,already_published,0);}
    catch(const std::future_error&){rejected=true;}
    {std::lock_guard<std::mutex> guard(mx->state);if(!rejected || mx->depth!=2 || mx->owner!=std::this_thread::get_id())return 1;}
    if(kernel_mutex_unlock(&mutex) || kernel_mutex_unlock(&mutex))return 1;
    auto other=std::async(std::launch::async,[&]{return kernel_mutex_lock(&mutex)==0 && kernel_mutex_unlock(&mutex)==0;});
    if(!other.get())return 1;
    std::cout<<"{\"scope\":\"isolated_condition_failure_reacquire\",\"injected_publication_failure\":true,\"owner_and_depth_restored_before_throw\":true,\"mutex_usable_after_cleanup\":true,\"guest_bound\":false}\n";return 0;
}
inline int condition_recursive_protocol_control() {
    std::uint64_t condition=0,mutex=0;lookup_condition(&condition);
    auto mx=lookup_mutex(&mutex,true);mx->type=2;
    if(kernel_mutex_lock(&mutex) || kernel_mutex_lock(&mutex) || kernel_mutex_lock(&mutex))return 1;
    std::promise<void> queued;auto ready=queued.get_future();
    auto broadcaster=std::async(std::launch::async,[&]{ready.get();
        return kernel_mutex_lock(&mutex)==0 && kernel_condition_broadcast(&condition)==0 && kernel_mutex_unlock(&mutex)==0;});
    if(!condition_mutex_wait_control(&condition,&mutex,queued) || !broadcaster.get())return 1;
    {std::lock_guard<std::mutex> guard(mx->state);if(mx->depth!=3 || mx->owner!=std::this_thread::get_id())return 1;}
    std::promise<void> timed;if(condition_mutex_wait_control(&condition,&mutex,timed,0))return 1;
    {std::lock_guard<std::mutex> guard(mx->state);if(mx->depth!=3)return 1;}
    if(kernel_mutex_unlock(&mutex) || kernel_mutex_unlock(&mutex) || kernel_mutex_unlock(&mutex) || kernel_mutex_unlock(&mutex)!=1)return 1;
    std::cout<<"{\"scope\":\"isolated_recursive_condition_protocol\",\"other_thread_acquired_during_wait\":true,\"wake_restored_depth\":3,\"timeout_restored_depth\":3,\"guest_bound\":false}\n";return 0;
}
inline int condition_timeout_reacquire_control() {
    std::uint64_t condition=0,mutex=0;lookup_condition(&condition);
    if(kernel_mutex_lock(&mutex))return 1;
    std::promise<void> queued;
    const bool woke=condition_mutex_wait_control(&condition,&mutex,queued,0);
    auto object=lookup_mutex(&mutex,false);bool owned=false;
    {std::lock_guard<std::mutex> guard(object->state);owned=object->owner==std::this_thread::get_id() && object->depth==1;}
    if(woke || !owned || kernel_mutex_unlock(&mutex))return 1;
    auto other=std::async(std::launch::async,[&]{return kernel_mutex_lock(&mutex)==0 && kernel_mutex_unlock(&mutex)==0;});
    if(!other.get())return 1;
    std::cout<<"{\"scope\":\"isolated_condition_timeout_reacquire\",\"zero_timeout_not_granted\":true,\"mutex_reacquired_before_return\":true,\"other_owner_after_unlock\":true,\"guest_wait_bound\":false}\n";return 0;
}
inline int condition_cancel_protocol_control() {
    std::uint64_t condition=0,mutex=0;lookup_condition(&condition);
    DeferredCancellationControl cancellation;std::promise<void> queued;auto ready=queued.get_future();
    auto worker=std::async(std::launch::async,[&]{
        if(kernel_mutex_lock(&mutex))return false;
        const bool woke=condition_mutex_wait_control(&condition,&mutex,queued);
        auto object=lookup_mutex(&mutex,false);bool owned=false;
        {std::lock_guard<std::mutex> guard(object->state);owned=object->owner==std::this_thread::get_id() && object->depth==1;}
        const bool delivered=owned && cancellation.cancellation_point();
        const bool once=!cancellation.cancellation_point();
        const bool released=kernel_mutex_unlock(&mutex)==0;cancellation.finish();
        return woke && owned && delivered && once && released;
    });ready.get();
    if(!cancellation.request() || kernel_condition_broadcast(&condition) || !worker.get() || cancellation.request())return 1;
    if(kernel_mutex_lock(&mutex) || kernel_mutex_unlock(&mutex))return 1;
    std::cout<<"{\"scope\":\"isolated_condition_cancel_protocol\",\"reacquire_before_delivery\":true,\"delivery_once\":true,\"mutex_usable_after_cleanup\":true,\"guest_bound\":false}\n";return 0;
}
inline int condition_mutex_protocol_control() {
    std::uint64_t condition=0,mutex=0;lookup_condition(&condition);
    std::promise<void> queued;auto ready=queued.get_future();
    auto worker=std::async(std::launch::async,[&]{
        if(kernel_mutex_lock(&mutex))return false;
        const bool woke=condition_mutex_wait_control(&condition,&mutex,queued);
        const auto object=lookup_mutex(&mutex,false);
        bool owned=false;{std::lock_guard<std::mutex> guard(object->state);owned=object->owner==std::this_thread::get_id() && object->depth==1;}
        return kernel_mutex_unlock(&mutex)==0 && woke && owned;
    });ready.get();
    if(kernel_mutex_lock(&mutex) || kernel_condition_broadcast(&condition) || kernel_mutex_unlock(&mutex) || !worker.get())return 1;
    std::promise<void> invalid;bool rejected=false;
    try{condition_mutex_wait_control(&condition,&mutex,invalid);}catch(const std::runtime_error&){rejected=true;}
    if(!rejected)return 1;
    std::cout<<"{\"scope\":\"isolated_condition_mutex_protocol\",\"released_before_wait\":true,\"broadcast_not_lost\":true,\"reacquired_before_return\":true,\"foreign_owner_rejected\":true,\"guest_bound\":false}\n";return 0;
}
// Owned opaque attributes: public shadPS4 condvar.cpp22-25,351-368,
// revision e3ce810f3a653f43ac64ebab63023de281a4103a (GPL-2.0-or-later).
struct ConditionAttributes { int process_shared=0; int clock_id=0; };
inline std::mutex condition_attributes_lock;
inline std::unordered_map<std::uint64_t,std::unique_ptr<ConditionAttributes>> condition_attributes;
inline int condition_attributes_init(std::uint64_t* cell) {
    if(!cell) return 0x80020016;
    require_time_output(cell,8);
    auto value=std::make_unique<ConditionAttributes>();
    const auto id=reinterpret_cast<std::uint64_t>(value.get());
    std::lock_guard<std::mutex> guard(condition_attributes_lock);
    condition_attributes.emplace(id,std::move(value));*cell=id;return 0;
}
inline int condition_attributes_destroy(std::uint64_t* cell) {
    if(!cell) return 0x80020016;
    require_time_output(cell,8);
    std::lock_guard<std::mutex> guard(condition_attributes_lock);
    if(!*cell || !condition_attributes.erase(*cell)) return 0x80020016;
    *cell=0;return 0;
}
inline int kernel_condition_init(std::uint64_t* cell,const std::uint64_t* attributes,const char* name) {
    if(!cell) return 0x80020016;
    require_time_output(cell,8);
    std::string label;
    // Public pinned condvar.cpp27-48 stores the optional label on each new object.
    // Read defensively; the bounded unsupported case never publishes a handle.
    if(name) {
        for(std::size_t i=0;i<4096;++i){char ch=0;SIZE_T got=0;
            if(!ReadProcessMemory(GetCurrentProcess(),name+i,&ch,1,&got) || got!=1)
                throw std::runtime_error("Unreadable condition name");
            if(!ch)break;
            label.push_back(ch);
            if(i==4095)throw std::runtime_error("Condition name exceeds characterized bound");
        }
    }
    if(attributes) {
        require_time_output(const_cast<std::uint64_t*>(attributes),8);
        std::lock_guard<std::mutex> guard(condition_attributes_lock);
        const auto found=condition_attributes.find(*attributes);
        if(found==condition_attributes.end()) throw std::runtime_error("Unknown condition attributes");
        if(found->second->process_shared || found->second->clock_id) throw std::runtime_error("Nondefault condition attributes unsupported");
    }
    auto object=std::make_shared<KernelCondition>();
    object->name=std::move(label);
    const auto id=reinterpret_cast<std::uint64_t>(object.get());
    std::lock_guard<std::mutex> guard(condition_registry_lock);
    condition_registry.emplace(id,object);*cell=id;return 0;
}
inline int condition_init_control() {
    std::uint64_t attr=0,cell=0;
    if(condition_attributes_init(&attr) || kernel_condition_init(&cell,&attr,nullptr) || !cell) throw std::runtime_error("Explicit condition init failed");
    auto object=lookup_condition(&cell);
    if(object->generation || kernel_condition_broadcast(&cell) || object->generation!=1) throw std::runtime_error("Explicit condition is not live");
    std::uint64_t unknown=1,untouched=0x1234;unsigned rejected=0;
    try {kernel_condition_init(&untouched,&unknown,nullptr);}catch(const std::runtime_error&){++rejected;}
    try {kernel_condition_init(&untouched,&attr,reinterpret_cast<const char*>(1));}catch(const std::runtime_error&){++rejected;}
    if(rejected!=2 || untouched!=0x1234 || condition_attributes_destroy(&attr)) throw std::runtime_error("Condition init negatives mutated output");
    if(kernel_condition_init(&untouched,nullptr,nullptr) || !untouched) throw std::runtime_error("Default condition init failed");
    std::uint64_t named=0,other=0;
    if(kernel_condition_init(&named,nullptr,"np worker") || kernel_condition_init(&other,nullptr,"np worker") || named==other || lookup_condition(&named)->name!="np worker") throw std::runtime_error("Named conditions must own independent labels and handles");
    if(kernel_condition_broadcast(&named) || lookup_condition(&named)->generation!=1 || lookup_condition(&other)->generation) throw std::runtime_error("Named condition isolation failed");
    if(kernel_condition_destroy(&named) || kernel_condition_destroy(&other)) throw std::runtime_error("Named condition release failed");
    std::cout<<"{\"scope\":\"condition_init_host\",\"live_broadcast\":true,\"default_and_owned_attributes\":true,\"negative_no_write\":2,\"guest_wait_supported\":false}\n";return 0;
}
inline int condition_attributes_control() {
    std::uint64_t cell=0;
    if(condition_attributes_init(&cell) || !cell) throw std::runtime_error("Condition attributes allocation failed");
    {
        std::lock_guard<std::mutex> guard(condition_attributes_lock);
        const auto& value=*condition_attributes.at(cell);
        if(value.process_shared || value.clock_id) throw std::runtime_error("Condition attribute defaults differ");
    }
    const auto stale=cell;
    if(condition_attributes_destroy(&cell) || cell) throw std::runtime_error("Condition attribute release failed");
    auto invalid=stale;
    if(condition_attributes_destroy(&invalid)!=int(0x80020016) || invalid!=stale ||
       condition_attributes_destroy(&cell)!=int(0x80020016) ||
       condition_attributes_init(nullptr)!=int(0x80020016)) throw std::runtime_error("Condition attribute negatives differ");
    std::cout<<"{\"scope\":\"condition_attributes_host\",\"defaults_verified\":true,\"release_verified\":true,\"negative_cases\":3,\"runtime_bound\":false}\n";
    return 0;
}
inline int kernel_condition_control() {
    std::uint64_t cell=0;
    if(kernel_condition_broadcast(&cell) || !cell) throw std::runtime_error("Static condition initialization failed");
    auto object=lookup_condition(&cell);
    std::promise<void> queued[2];
    std::future<void> workers[2];
    for(unsigned i=0;i<2;++i) workers[i]=std::async(std::launch::async,[&,i]{
        std::unique_lock<std::mutex> guard(object->state);
        const auto generation=object->generation;
        queued[i].set_value();
        if(!object->changed.wait_for(guard,std::chrono::seconds(2),[&]{return object->generation!=generation;})) throw std::runtime_error("Broadcast did not wake queued waiter");
    });
    for(auto& queue:queued) queue.get_future().wait();
    if(kernel_condition_broadcast(&cell)) throw std::runtime_error("Broadcast failed");
    for(auto& worker:workers) worker.get();
    unsigned rejected=0;
    for(auto invalid:{std::uint64_t(1),std::uint64_t(0x12345678)}) {
        try { (void)kernel_condition_broadcast(&invalid); } catch(const std::runtime_error&) {++rejected;}
    }
    if(rejected!=2) throw std::runtime_error("Unknown condition accepted");
    std::cout<<"{\"scope\":\"host_static_condition_broadcast\",\"queued_waiters_woken\":2,\"empty_broadcast_initialized\":true,\"unknown_handles_rejected\":2,\"guest_wait_service_implemented\":false,\"runtime_bound\":false,\"retail_executed\":false}\n";
    return 0;
}
}

namespace ctrport {
// Pinned condvar.cpp219-224,453-454: Orbis NID wraps relative microseconds, not timespec.
extern "C" int ctr_condition_timedwait(std::uint64_t* condition,std::uint64_t* mutex,std::uint64_t usec,std::uint64_t caller){
 try{const int result=kernel_condition_wait(condition,mutex,usec);std::printf("{\"scope\":\"owned_condition_timedwait\",\"nid\":\"BmMjYxmew1w\",\"library\":\"libkernel\",\"caller\":%llu,\"usec\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),static_cast<unsigned long long>(usec),result);std::fflush(stdout);return result;}
 catch(const std::exception& e){std::fprintf(stderr,"NID=BmMjYxmew1w library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_condition_timedwait();
inline int condition_timed_control(){
 std::uint64_t condition=0,mutex=0;auto cv=lookup_condition(&condition);auto mx=lookup_mutex(&mutex,true);mx->type=2;
 auto call=[&](std::uint64_t usec){return static_cast<int>(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_condition_timedwait),reinterpret_cast<std::uint64_t>(&condition),reinterpret_cast<std::uint64_t>(&mutex),usec));};
 if(call(0)!=int(0x80020001U) || kernel_mutex_lock(&mutex) || kernel_mutex_lock(&mutex))return 1;
 const auto start=std::chrono::steady_clock::now();if(call(20000)!=int(0x8002003cU) || std::chrono::steady_clock::now()-start<std::chrono::milliseconds(20))return 1;
 {std::lock_guard<std::mutex> lock(mx->state);if(mx->owner!=std::this_thread::get_id() || mx->depth!=2)return 1;}
 if(call(0)!=int(0x8002003cU))return 1;
 auto signaler=std::async(std::launch::async,[&]{if(kernel_mutex_lock(&mutex))return false;bool queued=false;{std::lock_guard<std::mutex> lock(cv->state);queued=cv->waiters==1;}kernel_condition_signal(&condition);return kernel_mutex_unlock(&mutex)==0 && queued;});
 if(call(2000000) || !signaler.get())return 1;
 {std::lock_guard<std::mutex> lock(mx->state);if(mx->owner!=std::this_thread::get_id() || mx->depth!=2)return 1;}
 if(kernel_mutex_unlock(&mutex) || kernel_mutex_unlock(&mutex))return 1;
 std::cout<<"{\"scope\":\"condition_timed_control\",\"SysV3\":true,\"real_timeout20ms\":true,\"zero_timeout\":true,\"real_signal_wakeup\":true,\"recursive_depth2_restored\":true,\"foreign_owner_eperm\":true}\n";return 0;
}
}
