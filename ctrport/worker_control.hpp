// SPDX-License-Identifier: GPL-2.0-only
// Isolated host control, not a guest pthread or scheduler implementation.
#pragma once
#include <functional>
#include <exception>
#include "thread_identity.hpp"
namespace ctrport {
struct WorkerControl {
    std::function<bool()> body;
    std::exception_ptr failure;
    bool passed=false;
    ULONG_PTR stack_low=0,stack_high=0,stack_marker=0;
    DWORD host_id=0;
    std::uint64_t runtime_identity=0;
};
inline DWORD WINAPI worker_control_entry(void* opaque) {
    auto& state=*static_cast<WorkerControl*>(opaque);
    GetCurrentThreadStackLimits(&state.stack_low,&state.stack_high);
    unsigned char local_marker=0;state.stack_marker=reinterpret_cast<ULONG_PTR>(&local_marker);
    state.host_id=GetCurrentThreadId();
    try {state.runtime_identity=thread_self();state.passed=state.body();} catch(...) {state.failure=std::current_exception();}
    try {if(state.runtime_identity && !release_thread_identity())throw std::runtime_error("Worker identity cleanup missing");} catch(...) {state.failure=std::current_exception();}
    return 0;
}
inline bool run_worker_control(std::function<bool()> body) {
    WorkerControl state;state.body=std::move(body);
    HANDLE handle=CreateThread(nullptr,65536,worker_control_entry,&state,STACK_SIZE_PARAM_IS_A_RESERVATION,nullptr);
    if(!handle) throw std::runtime_error("Owned Windows worker creation failed");
    const DWORD waited=WaitForSingleObject(handle,INFINITE);
    const bool closed=CloseHandle(handle)!=0;
    if(waited!=WAIT_OBJECT_0 || !closed) throw std::runtime_error("Owned Windows worker join failed");
    {std::lock_guard<std::mutex> guard(thread_identity_lock);if(state.runtime_identity && thread_identities.count(state.runtime_identity))throw std::runtime_error("Joined worker identity retained");}
    if(state.failure) std::rethrow_exception(state.failure);
    if(!state.stack_low || state.stack_high<=state.stack_low || state.stack_marker<state.stack_low || state.stack_marker>=state.stack_high || !state.host_id || state.host_id==GetCurrentThreadId()) throw std::runtime_error("Owned Windows worker stack/identity missing");
    std::cout<<"{\"scope\":\"owned_windows_worker_control\",\"stack_extent\":"<<(state.stack_high-state.stack_low)<<",\"local_marker_inside_stack\":true,\"identity_removed_after_join\":true,\"joined\":true,\"scheduler_equivalence\":false}\n";
    return state.passed;
}
inline bool run_published_worker_control(std::function<void(std::uint64_t)> publish,
                                         std::function<bool()> body,bool inject_before_identity=false) {
    std::promise<std::uint64_t> identity_ready;auto ready=identity_ready.get_future();
    std::promise<void> start;auto start_signal=start.get_future();
    auto running=std::async(std::launch::async,[&] {
        bool announced=false;
        try {
            if(inject_before_identity)throw std::runtime_error("Injected pre-identity failure");
            return run_worker_control([&] {
                identity_ready.set_value(thread_self());announced=true;
                start_signal.get();return body();
            });
        } catch(...) {
            if(!announced)identity_ready.set_exception(std::current_exception());
            throw;
        }
    });
    try {publish(ready.get());}
    catch(...) {
        auto failure=std::current_exception();start.set_exception(failure);
        try{running.get();}catch(...){}
        std::rethrow_exception(failure);
    }
    start.set_value();return running.get();
}
inline int publication_failure_control() {
    for(int mode:{0,1,2}) {
        bool entered=false,rejected=false;
        try {run_published_worker_control([&](std::uint64_t) {
            if(mode==1)throw std::runtime_error("Injected creator publication failure");
        },[&] {entered=true;if(mode==2)throw std::runtime_error("Injected worker body failure");return true;},mode==0);}
        catch(const std::runtime_error&){rejected=true;}
        if(!rejected || entered!=(mode==2))return 1;
    }
    if(!run_published_worker_control([](std::uint64_t id){if(!id)throw std::runtime_error("Identity missing");},[]{return true;}))return 1;
    std::cout<<"{\"scope\":\"synthetic_publication_failures\",\"pre_identity_failure_propagated\":true,\"creator_failure_unblocks_worker\":true,\"body_failure_joined\":true,\"subsequent_worker_passed\":true,\"guest_bound\":false}\n";return 0;
}

}
