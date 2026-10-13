// SPDX-License-Identifier: GPL-2.0-only
// Owned host launch context. Guest pthread binding and scheduling remain separate.
#pragma once
#include "guest_stack.hpp"
#include "tls_storage.hpp"
#include "thread_completion.hpp"
#include "thread_identity.hpp"
namespace ctrport {
// Own diagnostic identity, retained only while the original worker is executing.
inline thread_local void* observed_worker_argument=nullptr;
inline thread_local void* observed_worker_target=nullptr;
inline void* validated_worker_target(unsigned char* image,const ImageMetadata& metadata,const std::string& expected_sha,std::uint64_t offset,std::uint64_t size) {
    if(!image || metadata.sha256!=expected_sha || !size || offset>std::numeric_limits<std::uint64_t>::max()-size)throw std::runtime_error("Worker target identity/extent rejected");
    for(const auto& span:metadata.executable_spans)if(offset>=span.first && offset+size<=span.second)return image+offset;
    throw std::runtime_error("Worker target outside declared executable span");
}
class GuestWorkerControl {
    struct State {
        std::shared_ptr<const TlsStorage> tls;
        std::shared_ptr<void> image_owner;
        GuestStack stack;
        ThreadCompletion completion;
        void* target;void* argument;
        HANDLE ready=nullptr,start=nullptr;
        std::uint64_t identity=0;
        bool execute=false;
        int guest_priority=700;
        std::exception_ptr failure;
        State(std::shared_ptr<const TlsStorage> storage,std::shared_ptr<void> owner,void* entry,void* arg,std::size_t stack_size,std::size_t guard_size):tls(std::move(storage)),image_owner(std::move(owner)),stack(stack_size,guard_size),target(entry),argument(arg) {}
    };
    std::unique_ptr<State> state;
    HANDLE thread=nullptr;
    static DWORD WINAPI entry(void* opaque) {
        auto& s=*static_cast<State*>(opaque);
        try {
            s.identity=thread_self();
            if(!SetEvent(s.ready))throw std::runtime_error("Worker ready signal failed");
            if(WaitForSingleObject(s.start,INFINITE)!=WAIT_OBJECT_0)throw std::runtime_error("Worker start wait failed");
            if(s.execute) {
                observed_worker_argument=s.argument;observed_worker_target=s.target;
                current_guest_priority=s.guest_priority;
                std::uint64_t result=0;
                {TlsThreadLifetime lifetime(*s.tls);TlsContextScope context(*s.tls);
                 result=ctr_guest_stack_call(s.target,s.argument,s.stack.top(),s.stack.bottom());}
                s.completion.complete(result);
                observed_worker_argument=nullptr;observed_worker_target=nullptr;
            }
        }catch(...) {s.failure=std::current_exception();SetEvent(s.ready);}
        if(s.identity)release_thread_identity();
        return 0;
    }
public:
    GuestWorkerControl(std::shared_ptr<const TlsStorage> tls,std::shared_ptr<void> image_owner,void* target,void* argument,std::size_t stack_size=32768,std::size_t guard_size=4096) {
        if(!tls || !image_owner || !target)throw std::runtime_error("Worker requires retained TLS, image owner and target");
        state=std::make_unique<State>(std::move(tls),std::move(image_owner),target,argument,stack_size,guard_size);
        state->ready=CreateEventW(nullptr,TRUE,FALSE,nullptr);state->start=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if(!state->ready || !state->start) {if(state->ready)CloseHandle(state->ready);if(state->start)CloseHandle(state->start);throw std::runtime_error("Worker events failed");}
        thread=CreateThread(nullptr,65536,entry,state.get(),STACK_SIZE_PARAM_IS_A_RESERVATION,nullptr);
        if(!thread){const DWORD error=GetLastError();CloseHandle(state->ready);CloseHandle(state->start);throw std::runtime_error("Worker creation failed Win32="+std::to_string(error));}
        WaitForSingleObject(state->ready,INFINITE);
    }
    GuestWorkerControl(const GuestWorkerControl&)=delete;
    ~GuestWorkerControl(){if(thread){SetEvent(state->start);WaitForSingleObject(thread,INFINITE);CloseHandle(thread);}CloseHandle(state->ready);CloseHandle(state->start);}
    std::uint64_t identity()const{return state->identity;}
    void publish_and_start() {
        if(state->execute)throw std::runtime_error("Worker already started");
        if(state->failure)std::rethrow_exception(state->failure);
        state->completion.publish();state->execute=true;
        if(!SetEvent(state->start))throw std::runtime_error("Worker start signal failed");
    }
    void set_host_priority(int priority,int guest_priority=700) {
        if(state->execute)throw std::runtime_error("Priority must be applied before worker start");
        state->guest_priority=guest_priority;
        if(!SetThreadPriority(thread,priority) || GetThreadPriority(thread)!=priority)throw std::runtime_error("Windows worker priority application/readback failed");
    }
    void update_host_priority(int priority,int guest_priority){
        if(has_exited())throw std::runtime_error("Exited worker priority change");
        std::lock_guard<std::mutex> lock(thread_identity_lock);auto identity=thread_identities.find(state->identity);
        if(identity==thread_identities.end())throw std::runtime_error("Worker priority identity retired");
        if(!SetThreadPriority(thread,priority) || GetThreadPriority(thread)!=priority)throw std::runtime_error("Live Windows worker priority/readback failed");
        identity->second->priority_override.store(guest_priority);
    }
    DWORD_PTR set_host_affinity(DWORD_PTR mask) {
        if(!mask)throw std::runtime_error("Empty host worker affinity");
        if(GetActiveProcessorGroupCount()!=1)throw std::runtime_error("Worker affinity requires one host processor group");
        DWORD_PTR process=0,system=0;
        if(!GetProcessAffinityMask(GetCurrentProcess(),&process,&system) || (mask&~process))throw std::runtime_error("Worker host affinity outside process mask");
        if(has_exited())throw std::runtime_error("Exited worker affinity unsupported");
        const auto previous=SetThreadAffinityMask(thread,mask);
        if(!previous)throw std::runtime_error("Owned host worker affinity update failed");
        return previous;
    }
    bool has_exited() const {
        const auto result=WaitForSingleObject(thread,0);
        if(result==WAIT_FAILED)throw std::runtime_error("Worker exit query failed");
        return result==WAIT_OBJECT_0;
    }
    bool detach() {
        if(!state->execute)throw std::runtime_error("Unpublished worker cannot detach");
        return state->completion.detach();
    }
    bool has_joiner(){return state->completion.has_joiner();}
    bool completion_collected(){return state->completion.is_collected();}
    std::uint64_t join() {
        if(!state->execute)throw std::runtime_error("Unpublished worker cannot join");
        const auto joiner=thread_self();std::uint64_t value=0;
        if(joiner==state->identity)throw std::runtime_error("Worker cannot join itself");
        if(!state->completion.claim_join(joiner))throw std::runtime_error("Worker already has a joiner or was collected");
        try {
            if(WaitForSingleObject(thread,INFINITE)!=WAIT_OBJECT_0)throw std::runtime_error("Worker join failed");
            if(state->failure)std::rethrow_exception(state->failure);
            if(!state->completion.consume_join(joiner,value))throw std::runtime_error("Worker completion collection failed");
        }catch(...) {state->completion.backout_join(joiner);throw;}
        return value;
    }
};
struct WorkerJoinDetachGate {std::promise<void> ready;std::shared_future<void> release;};
extern "C" std::uint64_t ctr_worker_join_detach_gate(WorkerJoinDetachGate* gate) {gate->ready.set_value();gate->release.wait();return 17;}
extern "C" void ctr_worker_join_detach_fixture();
inline int guest_worker_join_detach_control() {
    auto tls=std::make_shared<TlsStorage>(std::vector<TlsTemplate>{{{7},8,8}});auto owner=std::make_shared<int>(91);
    std::promise<void> release;WorkerJoinDetachGate gate;gate.release=release.get_future().share();auto entered=gate.ready.get_future();
    GuestWorkerControl worker(tls,owner,reinterpret_cast<void*>(&ctr_worker_join_detach_fixture),&gate);worker.publish_and_start();
    const bool started=entered.wait_for(std::chrono::seconds(2))==std::future_status::ready;
    auto joiner=std::async(std::launch::async,[&]{const auto result=worker.join();release_thread_identity();return result;});
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!worker.has_joiner() && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
    const bool claimed=worker.has_joiner();const bool rejected=claimed && !worker.detach();
    release.set_value();const auto result=joiner.get();
    if(!started || !claimed || !rejected || result!=17 || !worker.completion_collected())return 1;
    std::cout<<"{\"scope\":\"owned_worker_join_detach_exclusion\",\"join_claim_observed_before_release\":true,\"detach_rejected_during_join\":true,\"result_collected_once\":true,\"guest_bound\":false}\n";return 0;
}
struct WorkerSelfJoinControl {GuestWorkerControl* worker=nullptr;bool rejected=false;};
extern "C" std::uint64_t ctr_worker_self_join_control(WorkerSelfJoinControl* value) {
    try{value->worker->join();}catch(const std::runtime_error&){value->rejected=true;}
    return 17;
}
extern "C" void ctr_worker_self_join_fixture();
inline int guest_worker_self_join_control() {
    auto tls=std::make_shared<TlsStorage>(std::vector<TlsTemplate>{{{7},8,8}});auto owner=std::make_shared<int>(91);
    WorkerSelfJoinControl state;GuestWorkerControl worker(tls,owner,reinterpret_cast<void*>(&ctr_worker_self_join_fixture),&state);state.worker=&worker;
    worker.publish_and_start();if(worker.join()!=17 || !state.rejected)return 1;
    std::cout<<"{\"scope\":\"owned_worker_self_join\",\"rejected_before_host_wait\":true,\"worker_returned\":true,\"parent_collected_result\":true,\"guest_bound\":false}\n";return 0;
}
extern "C" void ctr_worker_missing_fixture();
extern "C" unsigned char ctr_worker_missing_caller;
extern "C" void ctr_guest_fp_fixture();
extern "C" void ctr_host_fp_snapshot(void*);
inline int guest_worker_fp_control() {
    auto tls=std::make_shared<TlsStorage>(std::vector<TlsTemplate>{{{7},8,8}});
    auto owner=std::make_shared<int>(91);std::uint64_t guest=0,before=0,after=0;
    ctr_host_fp_snapshot(&before);
    {GuestWorkerControl worker(tls,owner,reinterpret_cast<void*>(&ctr_guest_fp_fixture),&guest);worker.publish_and_start();if(worker.join())return 1;}
    GuestStack stack(32768,4096);std::uint64_t direct=0;
    if(ctr_guest_stack_call(reinterpret_cast<void*>(&ctr_guest_fp_fixture),&direct,stack.top(),stack.bottom()))return 1;
    ctr_host_fp_snapshot(&after);
    if(guest!=0x037f00009fc0ULL || direct!=guest || before!=after)return 1;
    std::cout<<"{\"scope\":\"guest_worker_fp_context\",\"mxcsr\":40896,\"x87_control\":895,\"worker_and_direct_match\":true,\"host_restored\":true,\"guest_create_bound\":false}\n";return 0;
}
inline int guest_worker_missing_control() {
    auto tls=std::make_shared<TlsStorage>(std::vector<TlsTemplate>{{{7},8,8}});
    auto owner=std::make_shared<int>(91);std::uint64_t unknown=1;
    std::cout<<"{\"scope\":\"worker_missing_expected_caller\",\"caller\":\"0x"<<std::hex<<reinterpret_cast<std::uint64_t>(&ctr_worker_missing_caller)<<"\"}\n"<<std::flush;
    GuestWorkerControl worker(tls,owner,reinterpret_cast<void*>(&ctr_worker_missing_fixture),&unknown);
    worker.publish_and_start();worker.join();return 1;
}
inline int guest_worker_rollback_control() {
    auto tls=std::make_shared<TlsStorage>(std::vector<TlsTemplate>{{{7},8,8}});
    auto owner=std::make_shared<int>(91);std::weak_ptr<int> retained=owner;
    std::uint64_t rsp=0,aborted_identity=0;unsigned rejected=0;
    {GuestWorkerControl worker(tls,owner,reinterpret_cast<void*>(&ctr_guest_stack_fixture),&rsp);
     aborted_identity=worker.identity();owner.reset();
     try{worker.join();}catch(const std::runtime_error&){++rejected;}
    }
    if(rsp || !retained.expired())return 1;
    {std::lock_guard<std::mutex> guard(thread_identity_lock);if(thread_identities.count(aborted_identity))return 1;}
    owner=std::make_shared<int>(92);
    try{GuestWorkerControl invalid(nullptr,owner,reinterpret_cast<void*>(&ctr_guest_stack_fixture),&rsp);}catch(const std::runtime_error&){++rejected;}
    try{GuestWorkerControl invalid(tls,nullptr,reinterpret_cast<void*>(&ctr_guest_stack_fixture),&rsp);}catch(const std::runtime_error&){++rejected;}
    try{GuestWorkerControl invalid(tls,owner,nullptr,&rsp);}catch(const std::runtime_error&){++rejected;}
    {GuestWorkerControl worker(tls,owner,reinterpret_cast<void*>(&ctr_guest_stack_fixture),&rsp);
     worker.publish_and_start();
     try{worker.publish_and_start();}catch(const std::runtime_error&){++rejected;}
     if(worker.join()!=40)return 1;
     try{worker.join();}catch(const std::runtime_error&){++rejected;}
    }
    if(rejected!=6)return 1;
    std::cout<<"{\"scope\":\"owned_worker_rollback\",\"unpublished_target_not_entered\":true,\"identity_removed\":true,\"owner_released\":true,\"invalid_lifecycle_rejected\":6,\"subsequent_worker_returned\":true,\"guest_create_bound\":false}\n";return 0;
}
// An all-ones guest attribute is the documented unrestricted default, not
// a request for nonexistent Windows CPUs. Other explicit masks remain strict.
inline DWORD_PTR startup_host_affinity(std::uint64_t guest) {
    DWORD_PTR process=0,system=0;
    if(GetActiveProcessorGroupCount()!=1 || !GetProcessAffinityMask(GetCurrentProcess(),&process,&system) || !process)
        throw std::runtime_error("Worker process affinity unavailable");
    if(guest==~std::uint64_t{0})return process;
    if(!guest || (guest&~static_cast<std::uint64_t>(process)))
        throw std::runtime_error("Explicit guest affinity outside supported host domain");
    return static_cast<DWORD_PTR>(guest);
}
inline int guest_worker_host_affinity_control() {
    DWORD_PTR process=0,system=0;if(!GetProcessAffinityMask(GetCurrentProcess(),&process,&system) || !process)return 1;
    GROUP_AFFINITY before{},after{};if(!GetThreadGroupAffinity(GetCurrentThread(),&before))return 1;
    const auto selected=process&(~process+1);
    auto tls=std::make_shared<TlsStorage>(std::vector<TlsTemplate>{{{7},8,8}});auto owner=std::make_shared<int>(91);std::uint64_t rsp=0;
    GuestWorkerControl worker(tls,owner,reinterpret_cast<void*>(&ctr_guest_stack_fixture),&rsp);
    bool zero=false;try{worker.set_host_affinity(0);}catch(const std::runtime_error&){zero=true;}
    if(startup_host_affinity(~std::uint64_t{0})!=process || startup_host_affinity(selected)!=selected)return 1;
    bool empty_guest=false;try{startup_host_affinity(0);}catch(const std::runtime_error&){empty_guest=true;}
    if(!empty_guest)return 1;
    const auto original=worker.set_host_affinity(startup_host_affinity(~std::uint64_t{0}));
    worker.set_host_affinity(selected);
    if(worker.set_host_affinity(process)!=selected || worker.set_host_affinity(original)!=process || !zero)return 1;
    if(!GetThreadGroupAffinity(GetCurrentThread(),&after) || before.Mask!=after.Mask || before.Group!=after.Group)return 1;
    worker.publish_and_start();if(worker.join()!=40)return 1;
    bool exited=false;try{worker.set_host_affinity(selected);}catch(const std::runtime_error&){exited=true;}if(!exited)return 1;
    std::cout<<"{\"scope\":\"owned_worker_host_affinity\",\"foreign_worker_readback\":true,\"worker_mask_restored\":true,\"parent_unchanged\":true,\"zero_and_exited_rejected\":true,\"guest_mapping_claimed\":false}\n";return 0;
}
inline int guest_worker_detach_exited_control() {
    auto tls=std::make_shared<TlsStorage>(std::vector<TlsTemplate>{{{7},8,8}});
    auto owner=std::make_shared<int>(91);std::weak_ptr<int> retained=owner;std::uint64_t rsp=0;
    {GuestWorkerControl worker(tls,owner,reinterpret_cast<void*>(&ctr_guest_stack_fixture),&rsp);
     bool unpublished=false;try{worker.detach();}catch(const std::runtime_error&){unpublished=true;}
     if(!unpublished)return 1;worker.publish_and_start();owner.reset();
     const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
     while(!worker.has_exited()){if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Detach control worker deadline");std::this_thread::yield();}
     {std::lock_guard<std::mutex> guard(thread_identity_lock);if(thread_identities.count(worker.identity()))return 1;}
     if(!rsp || retained.expired() || worker.completion_collected() || !worker.detach() || !worker.completion_collected() || worker.detach())return 1;
     bool join_rejected=false;try{worker.join();}catch(const std::runtime_error&){join_rejected=true;}if(!join_rejected)return 1;
    }
    if(!retained.expired())return 1;
    std::cout<<"{\"scope\":\"owned_exited_worker_detach\",\"active_identity_removed\":true,\"completion_retained_until_detach\":true,\"double_detach_and_join_rejected\":true,\"owner_released\":true,\"guest_bound\":false}\n";return 0;
}
inline int guest_worker_competing_join_control() {
    auto tls=std::make_shared<TlsStorage>(std::vector<TlsTemplate>{{{7},8,8}});
    auto owner=std::make_shared<int>(91);std::uint64_t rsp=0;
    GuestWorkerControl worker(tls,owner,reinterpret_cast<void*>(&ctr_guest_stack_fixture),&rsp);
    worker.publish_and_start();
    std::promise<void> release;auto ready=release.get_future().share();
    auto attempt=[&] {ready.wait();bool collected=false;try{collected=worker.join()==40;}catch(const std::runtime_error&){}release_thread_identity();return collected;};
    auto first=std::async(std::launch::async,attempt);auto second=std::async(std::launch::async,attempt);
    release.set_value();const bool a=first.get(),b=second.get();if(a==b || !rsp)return 1;
    std::cout<<"{\"scope\":\"owned_worker_competing_join\",\"concurrent_joiners\":2,\"collections\":1,\"guest_bound\":false}\n";return 0;
}
inline int guest_worker_retention_control() {
    auto tls=std::make_shared<TlsStorage>(std::vector<TlsTemplate>{{{7},8,8}});
    auto owner=std::make_shared<int>(91);std::weak_ptr<int> retained=owner;
    std::uint64_t rsp=0;
    {GuestWorkerControl worker(tls,owner,reinterpret_cast<void*>(&ctr_guest_stack_fixture),&rsp);
     owner.reset();tls.reset();if(retained.expired() || !worker.identity() || rsp)return 1;
     worker.publish_and_start();if(worker.join()!=40 || !rsp || retained.expired())return 1;
     std::lock_guard<std::mutex> guard(thread_identity_lock);if(thread_identities.count(worker.identity()))return 1;
    }
    if(!retained.expired())return 1;
    std::cout<<"{\"scope\":\"owned_worker_retained_launch_context\",\"published_before_entry\":true,\"owner_retained_until_join\":true,\"identity_cleaned\":true,\"owner_released\":true,\"guest_create_bound\":false}\n";return 0;
}
}
