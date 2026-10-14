// SPDX-License-Identifier: GPL-2.0-only
// Independent owned stack allocation; public stack design audited in thread_create_stack_gap.json.
#pragma once
#include <limits>
#include "kernel_time.hpp"
namespace ctrport {
class GuestStack {
    void* allocation=nullptr;std::size_t reserved=0,usable=0,guard=0;
public:
    GuestStack(std::size_t requested,std::size_t requested_guard) {
        SYSTEM_INFO info{};GetSystemInfo(&info);const std::size_t page=info.dwPageSize;
        constexpr std::size_t allowance=128*1024,maximum=64*1024*1024;
        if(!requested || requested>maximum || requested_guard>maximum || !page)throw std::runtime_error("Guest stack extent unsupported");
        auto round=[page](std::size_t value){if(value>std::numeric_limits<std::size_t>::max()-(page-1))throw std::runtime_error("Guest stack extent overflow");return ((value+page-1)/page)*page;};
        usable=round(requested+allowance);guard=round(requested_guard);
        if(usable>maximum || guard>maximum-usable)throw std::runtime_error("Guest stack reservation exceeds bounded domain");
        reserved=usable+guard;
        allocation=VirtualAlloc(nullptr,reserved,MEM_RESERVE,PAGE_NOACCESS);
        if(!allocation)throw std::runtime_error("Guest stack reservation failed");
        if(!VirtualAlloc(static_cast<unsigned char*>(allocation)+guard,usable,MEM_COMMIT,PAGE_READWRITE)) {
            VirtualFree(allocation,0,MEM_RELEASE);allocation=nullptr;throw std::runtime_error("Guest stack commit failed");
        }
        try{publish_owned_output(this,bottom(),usable);}catch(...){VirtualFree(allocation,0,MEM_RELEASE);allocation=nullptr;throw;}
    }
    GuestStack(const GuestStack&)=delete;GuestStack& operator=(const GuestStack&)=delete;
    ~GuestStack(){revoke_owned_output(this);if(allocation)VirtualFree(allocation,0,MEM_RELEASE);}
    void* bottom()const{return static_cast<unsigned char*>(allocation)+guard;}
    void* top()const{return static_cast<unsigned char*>(allocation)+reserved;}
    void* base()const{return allocation;}
    std::size_t size()const{return usable;}
};
extern "C" std::uint64_t ctr_guest_stack_call(void*,void*,void*,void*);
// Guest thread entry with an exit frame for scePthreadExit (abi_probe.asm).
extern "C" std::uint64_t ctr_guest_thread_call(void*,void*,void*,void*,void**);
inline thread_local void* guest_thread_exit_frame=nullptr;
// scePthreadExit: returns the calling worker's exit frame (ctr_sysv_pthread_exit jumps to it).
// A thread without a published frame (the main thread) cannot exit this way: fail loudly.
extern "C" void* ctr_thread_exit_frame(std::uint64_t value,std::uint64_t caller){
 if(!guest_thread_exit_frame){std::fprintf(stderr,"NID=3kg7rT0NQIs library=libkernel caller=0x%llx unsupported: scePthreadExit outside a runtime worker thread\n",static_cast<unsigned long long>(caller));std::fflush(stderr);ExitProcess(4);}
 std::printf("{\"scope\":\"thread_exit\",\"nid\":\"3kg7rT0NQIs\",\"caller\":%llu,\"value\":%llu}\n",static_cast<unsigned long long>(caller),static_cast<unsigned long long>(value));std::fflush(stdout);
 return guest_thread_exit_frame;
}
extern "C" void ctr_sysv_pthread_exit();
extern "C" void ctr_guest_stack_fixture();
extern "C" int ctr_guest_stack_xmm_control(void*,void*,void*);
extern "C" void ctr_guest_stack_exception_fixture();
extern "C" unsigned char ctr_guest_stack_exception_site;
struct StackExceptionControl {std::uint64_t low=0,high=0;unsigned seen=0;};
inline thread_local StackExceptionControl* stack_exception_control=nullptr;
inline LONG CALLBACK guest_stack_control_handler(EXCEPTION_POINTERS* pointers) {
    auto* state=stack_exception_control;
    if(!state || pointers->ExceptionRecord->ExceptionCode!=EXCEPTION_ILLEGAL_INSTRUCTION || pointers->ContextRecord->Rip!=reinterpret_cast<DWORD64>(&ctr_guest_stack_exception_site))return EXCEPTION_CONTINUE_SEARCH;
    if(pointers->ContextRecord->Rsp<state->low || pointers->ContextRecord->Rsp>=state->high)return EXCEPTION_CONTINUE_SEARCH;
    ++state->seen;pointers->ContextRecord->Rip+=2;return EXCEPTION_CONTINUE_EXECUTION;
}
inline int guest_stack_exception_control() {
    GuestStack stack(32768,4096);StackExceptionControl state{reinterpret_cast<std::uint64_t>(stack.bottom()),reinterpret_cast<std::uint64_t>(stack.top()),0};
    auto* handler=AddVectoredExceptionHandler(1,guest_stack_control_handler);if(!handler)throw std::runtime_error("Stack exception control handler unavailable");
    EXCEPTION_RECORD record{};CONTEXT context{};EXCEPTION_POINTERS pointers{&record,&context};
    record.ExceptionCode=EXCEPTION_ILLEGAL_INSTRUCTION;context.Rip=reinterpret_cast<DWORD64>(&ctr_guest_stack_exception_site);context.Rsp=state.low+128;
    unsigned rejected=0;
    if(guest_stack_control_handler(&pointers)==EXCEPTION_CONTINUE_SEARCH)++rejected;
    stack_exception_control=&state;
    record.ExceptionCode=EXCEPTION_ACCESS_VIOLATION;
    if(guest_stack_control_handler(&pointers)==EXCEPTION_CONTINUE_SEARCH)++rejected;
    record.ExceptionCode=EXCEPTION_ILLEGAL_INSTRUCTION;++context.Rip;
    if(guest_stack_control_handler(&pointers)==EXCEPTION_CONTINUE_SEARCH)++rejected;
    --context.Rip;context.Rsp=state.high;
    if(guest_stack_control_handler(&pointers)==EXCEPTION_CONTINUE_SEARCH)++rejected;
    auto foreign=std::async(std::launch::async,[&]{return guest_stack_control_handler(&pointers)==EXCEPTION_CONTINUE_SEARCH;});
    if(foreign.get())++rejected;
    if(rejected!=5 || state.seen || context.Rip!=reinterpret_cast<DWORD64>(&ctr_guest_stack_exception_site)) {stack_exception_control=nullptr;RemoveVectoredExceptionHandler(handler);return 1;}
    const auto result=ctr_guest_stack_call(reinterpret_cast<void*>(&ctr_guest_stack_exception_fixture),nullptr,stack.top(),stack.bottom());
    stack_exception_control=nullptr;const bool removed=RemoveVectoredExceptionHandler(handler)!=0;
    std::cout<<"{\"scope\":\"synthetic_guest_stack_exception\",\"exceptions\":"<<state.seen<<",\"redzone_result\":"<<result<<",\"unknown_or_foreign_rejected\":5,\"handler_removed\":"<<(removed?"true":"false")<<",\"guest_create_bound\":false}\n";
    return state.seen==1 && result==40 && removed?0:1;
}
inline int guest_stack_call_control() {
    GuestStack stack(32768,4096);std::uint64_t guest_rsp=0;
    ULONG_PTR low_before=0,high_before=0,low_after=0,high_after=0;
    GetCurrentThreadStackLimits(&low_before,&high_before);
    const auto result=ctr_guest_stack_call(reinterpret_cast<void*>(&ctr_guest_stack_fixture),&guest_rsp,stack.top(),stack.bottom());
    const bool xmm_preserved=ctr_guest_stack_xmm_control(stack.top(),&guest_rsp,stack.bottom())==1;
    GetCurrentThreadStackLimits(&low_after,&high_after);
    if(!xmm_preserved || result!=40 || guest_rsp<reinterpret_cast<std::uint64_t>(stack.bottom())+128 || guest_rsp>=reinterpret_cast<std::uint64_t>(stack.top()) || guest_rsp%16!=8 || low_before!=low_after || high_before!=high_after)return 1;
    std::cout<<"{\"scope\":\"synthetic_sysv_owned_stack_call\",\"xmm6_to_15_preserved\":true,\"guest_stack_entered\":true,\"entry_alignment\":8,\"redzone_writes_returned\":true,\"host_stack_bounds_unchanged\":true,\"guest_create_bound\":false}\n";return 0;
}
inline int guest_stack_control() {
    void* released=nullptr;
    {GuestStack stack(32768,4096);released=stack.base();
     MEMORY_BASIC_INFORMATION lower{},body{};
     if(!VirtualQuery(stack.base(),&lower,sizeof(lower)) || !VirtualQuery(stack.bottom(),&body,sizeof(body)) || lower.State!=MEM_RESERVE || body.State!=MEM_COMMIT || body.Protect!=PAGE_READWRITE || stack.size()!=163840 || reinterpret_cast<std::uintptr_t>(stack.top())%16)throw std::runtime_error("Guest stack layout differs");
     auto* bytes=static_cast<unsigned char*>(stack.bottom());bytes[0]=17;bytes[stack.size()-1]=23;
     if(bytes[0]!=17 || bytes[stack.size()-1]!=23)return 1;
    }
    MEMORY_BASIC_INFORMATION freed{};if(!VirtualQuery(released,&freed,sizeof(freed)) || freed.State!=MEM_FREE)return 1;
    unsigned rejected=0;
    for(auto size:{std::size_t(0),std::numeric_limits<std::size_t>::max()})try{GuestStack invalid(size,4096);}catch(const std::runtime_error&){++rejected;}
    try{GuestStack invalid(32768,std::numeric_limits<std::size_t>::max());}catch(const std::runtime_error&){++rejected;}
    if(rejected!=3)return 1;
    std::cout<<"{\"scope\":\"owned_guest_stack_allocation\",\"retail_requested_bytes\":32768,\"usable_with_hle_allowance\":163840,\"guard_reserved\":true,\"released\":true,\"invalid_extents_rejected\":3,\"guest_create_bound\":false}\n";return 0;
}
}
