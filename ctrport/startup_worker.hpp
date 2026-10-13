// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "guest_worker.hpp"
#include <deque>
#include "thread_create_domain.hpp"
namespace ctrport {
inline std::shared_ptr<std::vector<MappedImage>> startup_worker_images;
inline std::shared_ptr<const TlsStorage> startup_worker_tls;
inline std::vector<ImageMetadata> startup_worker_metadata;
inline std::mutex startup_worker_lock;
inline std::vector<std::shared_ptr<GuestWorkerControl>> startup_workers;
inline std::shared_ptr<GuestWorkerControl> find_startup_worker(std::uint64_t identity) {
    if(!identity)return {};
    std::lock_guard<std::mutex> guard(startup_worker_lock);
    for(const auto& worker:startup_workers)if(worker->identity()==identity)return worker;
    return {};
}
inline std::size_t reap_startup_workers(std::size_t scan_limit=16) {
    if(!scan_limit || scan_limit>128)throw std::runtime_error("Worker reaper scan bound rejected");
    std::vector<std::shared_ptr<GuestWorkerControl>> retired;
    {std::lock_guard<std::mutex> guard(startup_worker_lock);std::size_t scanned=0;
     for(auto it=startup_workers.begin();it!=startup_workers.end() && scanned<scan_limit;++scanned) {
        if((*it)->has_exited() && (*it)->completion_collected()){retired.push_back(std::move(*it));it=startup_workers.erase(it);}else ++it;
     }}
    // Host handle/stack/image teardown happens after releasing the registry mutex.
    return retired.size();
}
inline int startup_worker_reaper_control() {
    auto tls=std::make_shared<TlsStorage>(std::vector<TlsTemplate>{{{7},8,8}});auto owner=std::make_shared<int>(91);std::weak_ptr<int> retained=owner;
    std::uint64_t rsp1=0,rsp2=0;
    auto joinable=std::make_shared<GuestWorkerControl>(tls,owner,reinterpret_cast<void*>(&ctr_guest_stack_fixture),&rsp1);
    auto detached=std::make_shared<GuestWorkerControl>(tls,owner,reinterpret_cast<void*>(&ctr_guest_stack_fixture),&rsp2);const auto joined_id=joinable->identity(),detached_id=detached->identity();
    {std::lock_guard<std::mutex> guard(startup_worker_lock);if(!startup_workers.empty())return 1;startup_workers={joinable,detached};}
    joinable->publish_and_start();detached->publish_and_start();if(!detached->detach())return 1;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!joinable->has_exited() || !detached->has_exited()){if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Reaper worker deadline");std::this_thread::yield();}
    auto lease=find_startup_worker(detached_id);detached.reset();owner.reset();tls.reset();
    if(reap_startup_workers(1)!=0 || reap_startup_workers(2)!=1 || find_startup_worker(detached_id) || !find_startup_worker(joined_id) || retained.expired())return 1;
    if(joinable->join()!=40 || reap_startup_workers(1)!=1 || find_startup_worker(joined_id))return 1;
    joinable.reset();if(retained.expired())return 1;lease.reset();if(!retained.expired() || !rsp1 || !rsp2)return 1;
    unsigned rejected=0;for(std::size_t bound:{std::size_t{0},std::size_t{129}})try{reap_startup_workers(bound);}catch(const std::runtime_error&){++rejected;}
    if(rejected!=2)return 1;
    std::cout<<"{\"scope\":\"startup_worker_bounded_reaper\",\"uncollected_joinable_retained\":true,\"scan_bound_observed\":true,\"detached_and_joined_reaped\":2,\"outstanding_lease_pins_owner\":true,\"invalid_bounds_rejected\":2,\"guest_bound\":false}\n";return 0;
}
// Independent join bridge, public shadPS4 pthread.cpp112-209 at
// e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later contract.
inline int join_startup_worker(std::uint64_t identity,std::uint64_t* output){
    if(!identity)return int(0x80020016U);
    if(identity==thread_self())return int(0x80020023U);
    auto worker=find_startup_worker(identity);if(!worker)return int(0x80020003U);
    if(output)require_time_output(output,8);
    if(worker->completion_collected())return int(0x80020016U);
    if(worker->has_joiner())return int(0x8002002dU);
    const auto value=worker->join();if(output)*output=value;
    std::cout<<"{\"scope\":\"owned_worker_join_return\",\"identity\":"<<identity<<",\"result_written\":"<<(output?"true":"false")<<"}\n"<<std::flush;
    return 0;
}
extern "C" int ctr_thread_join_owned(std::uint64_t identity,std::uint64_t* output,std::uint64_t caller){
    try{return join_startup_worker(identity,output);}catch(const std::exception& error){std::fprintf(stderr,"NID=onNY9Byn-W8 library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_thread_join_owned();
inline int startup_worker_join_bridge_control(){
    auto call=[](std::uint64_t id,std::uint64_t* output){return static_cast<int>(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_thread_join_owned),id,reinterpret_cast<std::uint64_t>(output)));};
    if(call(0,nullptr)!=int(0x80020016U) || call(~std::uint64_t(0),nullptr)!=int(0x80020003U) || call(thread_self(),nullptr)!=int(0x80020023U))return 1;
    auto tls=std::make_shared<TlsStorage>(std::vector<TlsTemplate>{{{7},8,8}});auto owner=std::make_shared<int>(91);
    std::promise<void> release;WorkerJoinDetachGate gate;gate.release=release.get_future().share();auto entered=gate.ready.get_future();
    auto worker=std::make_shared<GuestWorkerControl>(tls,owner,reinterpret_cast<void*>(&ctr_worker_join_detach_fixture),&gate);const auto id=worker->identity();
    {std::lock_guard<std::mutex> guard(startup_worker_lock);startup_workers.push_back(worker);}
    worker->publish_and_start();entered.wait();std::uint64_t output=91;
    auto joiner=std::async(std::launch::async,[&]{return call(id,&output);});
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!worker->has_joiner() && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
    const bool blocked=worker->has_joiner() && output==91 && joiner.wait_for(std::chrono::milliseconds(0))==std::future_status::timeout;
    release.set_value();if(joiner.get() || !blocked || output!=17 || call(id,nullptr)!=int(0x80020016U))return 1;
    std::uint64_t rsp=0;auto second=std::make_shared<GuestWorkerControl>(tls,owner,reinterpret_cast<void*>(&ctr_guest_stack_fixture),&rsp);
    {std::lock_guard<std::mutex> guard(startup_worker_lock);startup_workers.push_back(second);}
    second->publish_and_start();if(call(second->identity(),nullptr) || !second->completion_collected())return 1;
    {std::lock_guard<std::mutex> guard(startup_worker_lock);startup_workers.clear();}
    std::cout<<"{\"scope\":\"owned_worker_sysv_join\",\"real_block\":true,\"result17\":true,\"null_result\":true,\"single_collection\":true,\"invalid_self_unknown_rejected\":true}\n";return 0;
}
extern "C" int ctr_thread_detach_owned(std::uint64_t identity,std::uint64_t caller) {
    try {
        if(!identity)return static_cast<int>(0x80020016U);
        auto worker=find_startup_worker(identity);
        if(!worker)return static_cast<int>(0x80020003U);
        return worker->detach()?0:static_cast<int>(0x80020016U);
    }catch(const std::exception& error) {
        std::fprintf(stderr,"NID=4qGrR6eoP9Y library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);
    }
}
extern "C" void ctr_sysv_thread_detach_owned();
inline int startup_worker_detach_bridge_control() {
    auto call=[](std::uint64_t id){return static_cast<int>(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_thread_detach_owned),id,0));};
    if(call(0)!=static_cast<int>(0x80020016U) || call(~std::uint64_t{0})!=static_cast<int>(0x80020003U))return 1;
    auto tls=std::make_shared<TlsStorage>(std::vector<TlsTemplate>{{{7},8,8}});auto owner=std::make_shared<int>(91);std::uint64_t rsp=0;
    auto worker=std::make_shared<GuestWorkerControl>(tls,owner,reinterpret_cast<void*>(&ctr_guest_stack_fixture),&rsp);const auto id=worker->identity();
    {std::lock_guard<std::mutex> guard(startup_worker_lock);startup_workers.push_back(worker);}
    worker->publish_and_start();const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!worker->has_exited()){if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Detach bridge worker deadline");std::this_thread::yield();}
    if(call(id) || !worker->completion_collected() || call(id)!=static_cast<int>(0x80020016U))return 1;
    {std::lock_guard<std::mutex> guard(startup_worker_lock);startup_workers.clear();}
    if(call(id)!=static_cast<int>(0x80020003U))return 1;
    std::cout<<"{\"scope\":\"owned_worker_detach_sysv_bridge\",\"dead_worker_collected\":true,\"null_double_detach_einval\":true,\"unknown_removed_esrch\":true,\"retail_bound\":false}\n";return 0;
}
inline int startup_worker_retained_lookup_control() {
    auto tls=std::make_shared<TlsStorage>(std::vector<TlsTemplate>{{{7},8,8}});auto owner=std::make_shared<int>(91);std::weak_ptr<int> retained=owner;std::uint64_t rsp=0;
    auto worker=std::make_shared<GuestWorkerControl>(tls,owner,reinterpret_cast<void*>(&ctr_guest_stack_fixture),&rsp);const auto id=worker->identity();
    {std::lock_guard<std::mutex> guard(startup_worker_lock);if(!startup_workers.empty())return 1;startup_workers.push_back(worker);}
    worker->publish_and_start();const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!worker->has_exited()){if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Lookup control worker deadline");std::this_thread::yield();}
    {std::lock_guard<std::mutex> guard(thread_identity_lock);if(thread_identities.count(id))return 1;}
    auto lease=find_startup_worker(id);if(!lease || find_startup_worker(0) || find_startup_worker(id+1))return 1;
    {std::lock_guard<std::mutex> guard(startup_worker_lock);startup_workers.clear();}
    worker.reset();owner.reset();tls.reset();if(retained.expired() || find_startup_worker(id) || !lease->detach())return 1;
    lease.reset();if(!retained.expired() || !rsp)return 1;
    std::cout<<"{\"scope\":\"startup_retained_worker_lookup\",\"dead_worker_lookup\":true,\"lease_survives_registry_removal\":true,\"stale_lookup_rejected\":true,\"owner_released_after_last_lease\":true,\"guest_bound\":false}\n";return 0;
}
inline int set_owned_worker_priority(uint64_t handle,int priority){
 if(priority<256 || priority>767)return static_cast<int>(0x80020016U);
 auto worker=find_startup_worker(handle);if(!worker)return static_cast<int>(0x80020003U);
 if(worker->has_exited())return static_cast<int>(0x80020003U);
 ThreadAttributes attributes;attributes.priority=priority;worker->update_host_priority(startup_windows_priority(attributes),priority);return 0;
}
extern "C" int ctr_thread_set_priority(uint64_t handle,int priority,uint64_t caller){
 try{int rc=set_owned_worker_priority(handle,priority);std::printf("{\"scope\":\"original_PthreadSetprio_return\",\"caller\":%llu,\"priority\":%d,\"result\":%d,\"scheduler_equivalence\":false}\n",static_cast<unsigned long long>(caller),priority,rc);std::fflush(stdout);return rc;
 }catch(const std::exception& e){std::fprintf(stderr,"NID=W0Hpm2X0uPE library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}

// Independent Windows scheduling adapter; public pthread.cpp570-600 signature.
// Actual host priority change/readback; never claim PS4 FIFO/RR quantum equivalence.
inline int posix_set_sched(std::uint64_t handle,int policy,const int* parameter){
 if(!handle || !parameter)return 22;
 require_time_output(parameter,4);const int priority=*parameter;
 if((policy!=1 && policy!=3) || priority<256 || priority>767)return 22;
 std::shared_ptr<ThreadIdentity> identity;
 {std::lock_guard<std::mutex> lock(thread_identity_lock);const auto f=thread_identities.find(handle);if(f==thread_identities.end())return 3;identity=f->second;}
 ThreadAttributes attributes;attributes.priority=priority;const int host=startup_windows_priority(attributes);
 if(identity->host==std::this_thread::get_id()){
   if(!SetThreadPriority(GetCurrentThread(),host) || GetThreadPriority(GetCurrentThread())!=host)throw std::runtime_error("Current Windows priority application/readback failed");
   identity->priority_override.store(priority);
 }else{auto worker=find_startup_worker(handle);if(!worker || worker->has_exited())return 3;worker->update_host_priority(host,priority);}
 identity->guest_policy.store(policy);return 0;
}
extern "C" int ctr_posix_set_sched(std::uint64_t handle,int policy,const int* parameter,std::uint64_t caller){
 try{const auto result=posix_set_sched(handle,policy,parameter);std::printf("{\"scope\":\"owned_posix_setschedparam\",\"nid\":\"Xs9hdiD7sAA\",\"library\":\"libScePosix\",\"caller\":%llu,\"policy\":%d,\"priority\":%d,\"result\":%d,\"ps4_scheduler_equivalence\":false}\n",static_cast<unsigned long long>(caller),policy,(result==0 && parameter)?*parameter:-1,result);std::fflush(stdout);return result;}
 catch(const std::exception& e){std::fprintf(stderr,"NID=Xs9hdiD7sAA library=libScePosix caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_posix_set_sched();
inline int posix_set_sched_control(){
 const auto self=thread_self();const auto old_host=GetThreadPriority(GetCurrentThread());const int old_guest=effective_guest_priority();int priority=478;
 auto call=[&](int policy){return ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_posix_set_sched),self,policy,reinterpret_cast<std::uint64_t>(&priority));};
 if(call(3) || effective_guest_priority()!=478 || GetThreadPriority(GetCurrentThread())!=THREAD_PRIORITY_NORMAL)return 1;
 priority=700;if(call(1) || GetThreadPriority(GetCurrentThread())!=THREAD_PRIORITY_LOWEST || effective_guest_priority()!=700)return 1;
 const int before=GetThreadPriority(GetCurrentThread());priority=768;
 if(call(3)!=22 || GetThreadPriority(GetCurrentThread())!=before || posix_set_sched(1,3,&priority)!=22 || posix_set_sched(self,3,nullptr)!=22)return 1;
 priority=478;if(posix_set_sched(1,3,&priority)!=3)return 1;
 if(!SetThreadPriority(GetCurrentThread(),old_host))return 1;
 {std::lock_guard<std::mutex> lock(thread_identity_lock);thread_identities.at(self)->priority_override.store(old_guest);thread_identities.at(self)->guest_policy.store(1);}
 std::cout<<"{\"scope\":\"posix_setschedparam_control\",\"actual_host_readbacks\":2,\"SysV3\":true,\"invalid_no_change\":true,\"ps4_scheduler_equivalence\":false}\n";return 0;
}
extern "C" void ctr_sysv_thread_set_priority();
inline int live_worker_priority_control(){
 auto tls=std::make_shared<TlsStorage>(std::vector<TlsTemplate>{{{7},8,8}});auto owner=std::make_shared<int>(91);
 std::promise<void> release;WorkerJoinDetachGate gate;gate.release=release.get_future().share();auto entered=gate.ready.get_future();
 auto worker=std::make_shared<GuestWorkerControl>(tls,owner,reinterpret_cast<void*>(&ctr_worker_join_detach_fixture),&gate);
 {std::lock_guard<std::mutex> lock(startup_worker_lock);if(!startup_workers.empty())return 1;startup_workers.push_back(worker);}
 worker->publish_and_start();entered.wait();const auto id=worker->identity();
 const int rc=static_cast<int>(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_thread_set_priority),id,260));
 int value=-1;{std::lock_guard<std::mutex> lock(thread_identity_lock);value=thread_identities.at(id)->priority_override.load();}
 const bool valid=rc==0 && value==260 && set_owned_worker_priority(id,768)==static_cast<int>(0x80020016U) && set_owned_worker_priority(0,260)==static_cast<int>(0x80020003U);
 release.set_value();if(worker->join()!=17 || !valid || set_owned_worker_priority(id,260)!=static_cast<int>(0x80020003U))return 1;
 {std::lock_guard<std::mutex> lock(startup_worker_lock);startup_workers.clear();}
 std::cout<<"{\"scope\":\"live_worker_priority_control\",\"SysV2\":true,\"host_priority_readback\":true,\"shared_guest_priority\":260,\"negative_cases\":3,\"scheduler_equivalence\":false,\"passed\":true}\n";return 0;
}
inline int create_owned_startup_thread(std::uint64_t* output,const std::uint64_t* attributes,void* target,void* argument,const char* name,std::uint64_t caller,bool posix) {
    try {
        validate_startup_thread_output(output);
        auto value=snapshot_create_attributes(attributes);
        if(value.inherit==4) {value.inherit=0;value.priority=effective_guest_priority();}
        std::cout<<"{\"scope\":\"startup_create_actual_attributes\",\"policy\":"<<value.policy<<",\"priority\":"<<value.priority<<",\"inherit\":"<<value.inherit<<",\"stack_size\":"<<value.stack_size<<",\"guard_size\":"<<value.guard_size<<",\"detached\":"<<value.detached<<",\"suspended\":"<<value.suspended<<",\"system_scope\":"<<value.system_scope<<"}\n"<<std::flush;
        if(startup_worker_images && !startup_worker_images->empty() && target==startup_worker_images->front().get()+0x1409af0) {
            const auto address=reinterpret_cast<std::uintptr_t>(argument);
            if(!address || address>std::numeric_limits<std::uintptr_t>::max()-0x5c)throw std::runtime_error("Original worker selector address rejected");
            int selector=0;SIZE_T read=0;
            if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address+0x58),&selector,sizeof(selector),&read) || read!=sizeof(selector))throw std::runtime_error("Original worker selector unreadable");
            std::cout<<"{\"scope\":\"original_startup_affinity_selector\",\"object_offset\":88,\"selector\":"<<selector<<",\"worker_entered\":false}\n"<<std::flush;
        }
        validate_startup_create_domain(value);
        if(!startup_worker_images || !startup_worker_tls)throw std::runtime_error("Startup worker context missing");
        if(startup_worker_metadata.empty())throw std::runtime_error("Worker image metadata missing");
        const auto target_image=resolve_owned_callback(*startup_worker_images,startup_worker_metadata,startup_worker_metadata.front().sha256,target);
        auto& image=(*startup_worker_images)[target_image];
        std::cout<<"{\"scope\":\"startup_callback_image\",\"image_sha256\":\""<<startup_worker_metadata[target_image].sha256<<"\",\"entry_offset\":"<<(static_cast<const unsigned char*>(target)-image.get())<<"}\n"<<std::flush;
        (void)name; // Naming has no guest execution effect; do not read an untrusted string.
        auto worker=std::make_shared<GuestWorkerControl>(startup_worker_tls,startup_worker_images,target,argument,static_cast<std::size_t>(value.stack_size),static_cast<std::size_t>(value.guard_size));
        worker->set_host_priority(startup_windows_priority(value),value.priority);
        // Unrestricted default uses the actual process mask; explicit masks stay strict.
        if(value.cpu_set_size){
            const auto host=startup_host_affinity(value.cpu_set);
            worker->set_host_affinity(host);
            std::cout<<"{\"scope\":\"startup_affinity_applied\",\"guest_mask\":"<<value.cpu_set<<",\"host_mask\":"<<host<<",\"unrestricted_default\":"<<(value.cpu_set==~std::uint64_t{0}?"true":"false")<<"}\n"<<std::flush;
        }
        const auto identity=worker->identity();
        if(!identity)throw std::runtime_error("Worker identity unavailable");
        if(value.cpu_set_size)record_thread_affinity(identity,value.cpu_set);
        std::lock_guard<std::mutex> guard(startup_worker_lock);
        startup_workers.push_back(std::move(worker));
        *output=identity;
        std::cout<<"{\"scope\":\"original_startup_worker_publish\",\"entry_offset\":"<<static_cast<const unsigned char*>(target)-image.get()<<",\"identity\":"<<identity<<",\"handle_before_start\":true,\"scheduler_equivalence\":false}\n"<<std::flush;
        startup_workers.back()->publish_and_start();
        if(value.detached && !startup_workers.back()->detach())throw std::runtime_error("Initial worker detach transition failed");
        return 0;
    }catch(const std::exception& error) {
        std::fprintf(stderr,"NID=%s library=%s caller=0x%llx unsupported: %s\n",posix?"Jmi+9w9u0E4":"6UgtwV+0zb4",posix?"libScePosix":"libkernel",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);
    }
}
// Independent registration storage; public net_ctl_obj.cpp32-46, net_ctl_obj.h
// eight-slot contract. GPL-2.0-or-later reference; no callback dispatch claimed.
struct NetToolkitCallback {void* function=nullptr;void* argument=nullptr;};
inline std::mutex net_toolkit_callback_lock;
inline std::array<NetToolkitCallback,8> net_toolkit_callbacks{};
inline int register_net_toolkit_callback(void* function,void* argument,int* output){
    if(!function || !output)return static_cast<int>(0x80412107U);
    require_time_output(output,sizeof(*output));
    std::lock_guard<std::mutex> lock(net_toolkit_callback_lock);
    for(std::size_t i=0;i<net_toolkit_callbacks.size();++i)if(!net_toolkit_callbacks[i].function){
        net_toolkit_callbacks[i]={function,argument};*output=static_cast<int>(i);return 0;
    }
    return static_cast<int>(0x80412103U);
}
extern "C" int ctr_net_toolkit_register(void* function,void* argument,int* output,std::uint64_t caller){
    try{
        if(function && output){
            if(!startup_worker_images || startup_worker_metadata.empty())throw std::runtime_error("Callback image ownership unavailable");
            resolve_owned_callback(*startup_worker_images,startup_worker_metadata,startup_worker_metadata.front().sha256,function);
        }
        const int result=register_net_toolkit_callback(function,argument,output);
        std::printf("{\"scope\":\"net_toolkit_callback_register\",\"caller\":%llu,\"result\":%d,\"id\":%d}\n",static_cast<unsigned long long>(caller),result,result? -1:*output);std::fflush(stdout);return result;
    }catch(const std::exception& e){std::fprintf(stderr,"NID=wIsKy+TfeLs library=libSceNetCtlForNpToolkit caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
// Public net_ctl_obj.cpp59-70 dispatches the actual offline disconnected
// condition (event1) to retained callbacks. No online state is synthesized.
inline void dispatch_net_toolkit_offline(const std::array<NetToolkitCallback,8>& callbacks){
    for(const auto& cb:callbacks)if(cb.function)
        ctr_call_original_integer2(cb.function,1,reinterpret_cast<std::uint64_t>(cb.argument));
}
extern "C" int ctr_net_toolkit_check(std::uint64_t caller){
    try{
        std::array<NetToolkitCallback,8> snapshot;
        {std::lock_guard<std::mutex> lock(net_toolkit_callback_lock);snapshot=net_toolkit_callbacks;}
        for(const auto& cb:snapshot)if(cb.function){
            if(!startup_worker_images || startup_worker_metadata.empty())throw std::runtime_error("Callback image owner unavailable");
            resolve_owned_callback(*startup_worker_images,startup_worker_metadata,startup_worker_metadata.front().sha256,cb.function);
        }
        dispatch_net_toolkit_offline(snapshot);
        std::printf("{\"scope\":\"net_toolkit_offline_callback_dispatch\",\"caller\":%llu,\"event\":1,\"result\":0}\n",static_cast<unsigned long long>(caller));std::fflush(stdout);return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"NID=u5oqtlIP+Fw library=libSceNetCtlForNpToolkit caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_offline_callback_fixture();
inline int net_toolkit_dispatch_control(){
    std::array<int,2> first{91,0},second{92,0};std::array<NetToolkitCallback,8> callbacks{};
    callbacks[0]={reinterpret_cast<void*>(&ctr_sysv_offline_callback_fixture),first.data()};
    callbacks[7]={reinterpret_cast<void*>(&ctr_sysv_offline_callback_fixture),second.data()};
    dispatch_net_toolkit_offline(callbacks);dispatch_net_toolkit_offline(callbacks);
    if(first!=std::array<int,2>{1,2} || second!=std::array<int,2>{1,2})return 1;
    std::cout<<"{\"scope\":\"net_toolkit_offline_dispatch_control\",\"actual_SysV_callbacks\":4,\"event\":1,\"context_identity\":true}\n";return 0;
}
inline int net_toolkit_register_control(){
    int output=91;int argument=19;
    if(register_net_toolkit_callback(nullptr,&argument,&output)!=static_cast<int>(0x80412107U) || output!=91)return 1;
    for(int i=0;i<8;++i){auto function=reinterpret_cast<void*>(static_cast<std::uintptr_t>(i+1));
        if(register_net_toolkit_callback(function,&argument,&output) || output!=i || net_toolkit_callbacks[i].function!=function || net_toolkit_callbacks[i].argument!=&argument)return 1;
    }
    output=91;if(register_net_toolkit_callback(&argument,nullptr,&output)!=static_cast<int>(0x80412103U) || output!=91)return 1;
    net_toolkit_callbacks={};
    std::cout<<"{\"scope\":\"net_toolkit_registration_control\",\"slots\":8,\"retained_arguments\":true,\"full_and_null_preserve_output\":true,\"callback_executed\":false}\n";return 0;
}
extern "C" int ctr_thread_create(std::uint64_t* output,const std::uint64_t* attributes,void* target,void* argument,const char* name,std::uint64_t caller) {return create_owned_startup_thread(output,attributes,target,argument,name,caller,false);}
extern "C" int ctr_posix_thread_create(std::uint64_t* output,const std::uint64_t* attributes,void* target,void* argument,const char* name,std::uint64_t caller) {return create_owned_startup_thread(output,attributes,target,argument,name,caller,true);}
extern "C" void ctr_sysv_posix_thread_create();
extern "C" void ctr_sysv_thread_create();extern "C" int ctr_user_callback_register(void* function,void* argument,std::uint64_t caller){
    try{
        if(!startup_worker_images || startup_worker_metadata.empty())throw std::runtime_error("User callback image owner unavailable");
        const auto index=resolve_owned_callback(*startup_worker_images,startup_worker_metadata,startup_worker_metadata.front().sha256,function);
        const auto offset=static_cast<unsigned char*>(function)-(*startup_worker_images)[index].get();
        const int result=user_callback_register_owned(function,argument);
        std::printf("{\"scope\":\"user_callback_registered\",\"caller\":%llu,\"image_sha256\":\"%s\",\"offset\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),startup_worker_metadata[index].sha256.c_str(),static_cast<unsigned long long>(offset),result);std::fflush(stdout);return result;
    }catch(const std::exception& e){std::fprintf(stderr,"NID=wuI7c7UNk0A library=libSceUserServiceForNpToolkit caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}

// Independent state storage: public pinned np_manager.cpp1049-1067,
// GPL-2.0-or-later. Registration lifecycle only; no online state fabricated.
inline std::mutex np_toolkit_callback_lock;
inline void* np_toolkit_callback=nullptr;
inline void* np_toolkit_argument=nullptr;
inline int np_toolkit_register(void* function,void* argument){
    std::lock_guard<std::mutex> guard(np_toolkit_callback_lock);
    np_toolkit_callback=function;np_toolkit_argument=argument;return 0;
}
inline int np_toolkit_unregister(){
    std::lock_guard<std::mutex> guard(np_toolkit_callback_lock);
    if(!np_toolkit_callback)return static_cast<int>(0x80550009U);
    np_toolkit_callback=nullptr;np_toolkit_argument=nullptr;return 0;
}
extern "C" int ctr_np_toolkit_register(void* function,void* argument,std::uint64_t caller){
    try{
        if(function){
            if(!startup_worker_images || startup_worker_metadata.empty())throw std::runtime_error("NP callback image ownership unavailable");
            resolve_owned_callback(*startup_worker_images,startup_worker_metadata,startup_worker_metadata.front().sha256,function);
        }
        const int rc=np_toolkit_register(function,argument);
        std::printf("{\"scope\":\"np_toolkit_callback_register\",\"caller\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),rc);std::fflush(stdout);return rc;
    }catch(const std::exception& e){std::fprintf(stderr,"NID=0c7HbXRKUt4 library=libSceNpManagerForToolkit caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" int ctr_np_toolkit_unregister(std::uint64_t caller){
    const int rc=np_toolkit_unregister();std::printf("{\"scope\":\"np_toolkit_callback_unregister\",\"caller\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),rc);std::fflush(stdout);return rc;
}
// Public np_manager.cpp902-979: drain queued state events, dispatch snapshot
// on calling thread. Offline runtime has no PSN state producer; never invent one.
inline std::deque<std::pair<int,int>> np_toolkit_pending_events;
inline std::size_t dispatch_np_toolkit_pending(){
    std::deque<std::pair<int,int>> pending;void* callback=nullptr;void* argument=nullptr;
    {std::lock_guard<std::mutex> lock(np_toolkit_callback_lock);pending.swap(np_toolkit_pending_events);callback=np_toolkit_callback;argument=np_toolkit_argument;}
    for(const auto& event:pending)if(callback)
        ctr_call_original_integer3(callback,static_cast<std::uint64_t>(event.first),static_cast<std::uint64_t>(event.second),reinterpret_cast<std::uint64_t>(argument));
    return pending.size();
}
inline int np_check_pending(std::uint64_t caller,const char* nid,const char* library,const char* scope){
    try{
        {std::lock_guard<std::mutex> lock(np_toolkit_callback_lock);
         if(np_toolkit_callback && !np_toolkit_pending_events.empty()){
            if(!startup_worker_images || startup_worker_metadata.empty())throw std::runtime_error("NP callback image owner unavailable");
            resolve_owned_callback(*startup_worker_images,startup_worker_metadata,startup_worker_metadata.front().sha256,np_toolkit_callback);
         }}
        const auto count=dispatch_np_toolkit_pending();
        std::printf("{\"scope\":\"%s\",\"caller\":%llu,\"events_consumed\":%llu,\"result\":0}\n",scope,static_cast<unsigned long long>(caller),static_cast<unsigned long long>(count));std::fflush(stdout);return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"NID=%s library=%s caller=0x%llx unsupported: %s\n",nid,library,static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" int ctr_np_toolkit_check(std::uint64_t caller){return np_check_pending(caller,"JELHf4xPufo","libSceNpManagerForToolkit","np_toolkit_event_queue_checked");}
// Public np_manager.cpp964-973: same state queue plus registered async jobs.
// Async request/registration producers remain missing and cannot create jobs;
// do not manufacture completion or online state in this offline domain.
extern "C" int ctr_np_check(std::uint64_t caller){return np_check_pending(caller,"3Zl8BePTh9Y","libSceNpManager","np_general_event_queue_checked");}
extern "C" void ctr_sysv_np_check();
inline int np_general_check_control(){
    if(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_np_check),0,0))return 1;
    if(!np_toolkit_pending_events.empty())return 1;
    std::cout<<"{\"scope\":\"np_general_check_control\",\"actual_SysV_empty_queue\":true,\"async_producers_unbound\":true}\n";return 0;
}
extern "C" void ctr_sysv_np_callback_fixture();
inline int np_toolkit_dispatch_control(){
    std::array<int,3> output{91,92,0};
    np_toolkit_register(reinterpret_cast<void*>(&ctr_sysv_np_callback_fixture),output.data());
    {std::lock_guard<std::mutex> lock(np_toolkit_callback_lock);np_toolkit_pending_events.emplace_back(7,0);np_toolkit_pending_events.emplace_back(9,1);}
    if(dispatch_np_toolkit_pending()!=2 || output!=std::array<int,3>{9,1,2} || dispatch_np_toolkit_pending()!=0 || output[2]!=2 || np_toolkit_unregister())return 1;
    std::cout<<"{\"scope\":\"np_toolkit_event_dispatch_control\",\"authored_events\":2,\"actual_SysV3_callbacks\":2,\"consume_once\":true,\"runtime_events_fabricated\":false}\n";return 0;
}
inline int np_toolkit_callback_control(){
    int first=1,second=2;
    if(np_toolkit_unregister()!=static_cast<int>(0x80550009U) || np_toolkit_register(&first,&second) || np_toolkit_callback!=&first || np_toolkit_argument!=&second)return 1;
    if(np_toolkit_register(&second,&first) || np_toolkit_callback!=&second || np_toolkit_argument!=&first || np_toolkit_unregister() || np_toolkit_callback || np_toolkit_argument)return 1;
    if(np_toolkit_unregister()!=static_cast<int>(0x80550009U) || np_toolkit_register(nullptr,nullptr) || np_toolkit_unregister()!=static_cast<int>(0x80550009U))return 1;
    std::cout<<"{\"scope\":\"np_toolkit_callback_control\",\"registration_replacement_and_release\":true,\"stale_error\":true,\"callback_executed\":false}\n";return 0;
}

}

