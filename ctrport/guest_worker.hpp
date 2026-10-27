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
                 result=ctr_guest_thread_call(s.target,s.argument,s.stack.top(),s.stack.bottom(),&guest_thread_exit_frame);}
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
extern "C" void ctr_thread_exit_fixture();
struct WorkerSelfJoinControl {GuestWorkerControl* worker=nullptr;bool rejected=false;};
extern "C" std::uint64_t ctr_worker_self_join_control(WorkerSelfJoinControl* value) {
    try{value->worker->join();}catch(const std::runtime_error&){value->rejected=true;}
    return 17;
}
extern "C" void ctr_worker_self_join_fixture();
extern "C" void ctr_worker_missing_fixture();
extern "C" unsigned char ctr_worker_missing_caller;
extern "C" void ctr_guest_fp_fixture();
extern "C" void ctr_host_fp_snapshot(void*);
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
}
