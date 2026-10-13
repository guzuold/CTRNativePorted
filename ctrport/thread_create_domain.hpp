// SPDX-License-Identifier: GPL-2.0-only
// Bounded startup attributes and explicit Windows priority mapping.
#pragma once
#include "thread_attributes.hpp"
namespace ctrport {
inline void validate_startup_thread_output(std::uint64_t* output) {
    if(!output)throw std::runtime_error("Thread output handle is required");
    require_time_output(output,sizeof(*output));
}
inline int thread_create_output_control() {
    std::uint64_t output=91;validate_startup_thread_output(&output);if(output!=91)return 1;
    SYSTEM_INFO info{};GetSystemInfo(&info);const auto page=static_cast<std::size_t>(info.dwPageSize);
    auto memory=static_cast<unsigned char*>(VirtualAlloc(nullptr,page*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!memory)throw std::runtime_error("Output control allocation failed");
    DWORD old=0;if(!VirtualProtect(memory+page,page,PAGE_READONLY,&old)){VirtualFree(memory,0,MEM_RELEASE);throw std::runtime_error("Output control protection failed");}
    unsigned rejected=0;
    for(auto pointer:{static_cast<std::uint64_t*>(nullptr),reinterpret_cast<std::uint64_t*>(memory+page),reinterpret_cast<std::uint64_t*>(memory+page-4)}) {
        try{validate_startup_thread_output(pointer);}catch(const std::runtime_error&){++rejected;}
    }
    VirtualFree(memory,0,MEM_RELEASE);if(rejected!=3)return 1;
    std::cout<<"{\"scope\":\"startup_thread_output_validation\",\"null_readonly_crosspage_rejected\":3,\"valid_output_unchanged\":true,\"worker_created\":false}\n";return 0;
}
inline ThreadAttributes snapshot_create_attributes(const std::uint64_t* cell) {
    if(!cell)return ThreadAttributes{};
    require_time_output(cell,8);
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    if(!*cell)return ThreadAttributes{};
    const auto found=thread_attributes.find(*cell);
    if(found==thread_attributes.end())throw std::runtime_error("Unknown create attribute handle");
    return *found->second;
}
inline void validate_startup_create_domain(const ThreadAttributes& value) {
    if(value.inherit!=0)throw std::runtime_error("Inherited scheduling attribute snapshot missing");
    if(value.policy!=1 || (value.priority<256 || value.priority>767))throw std::runtime_error("Nondefault guest scheduling domain missing");
    if(value.suspended || !value.system_scope)throw std::runtime_error("Suspended/process-scope worker missing");
    if(value.stack_address || (value.cpu_set_size && (value.cpu_set_size!=8 || !value.cpu_set)))throw std::runtime_error("User stack or explicit CPU set worker missing");
    if(!value.stack_size || value.stack_size>64*1024*1024 || value.guard_size>64*1024*1024)throw std::runtime_error("Worker stack domain rejected");
}
// Own monotonic five-bucket policy: lower guest priority number is higher.
// FIFO/RR domain256..767: 256..358 highest,359..460 above normal,
// 461..563 normal,564..665 below normal,666..767 lowest.
// Windows performs scheduling; no PS4 timing/order equivalence is claimed.
inline int startup_windows_priority(const ThreadAttributes& value) {
    if(value.policy!=1 || value.priority<256 || value.priority>767)throw std::runtime_error("Unsupported Windows priority mapping domain");
    const int priorities[]={THREAD_PRIORITY_HIGHEST,THREAD_PRIORITY_ABOVE_NORMAL,THREAD_PRIORITY_NORMAL,THREAD_PRIORITY_BELOW_NORMAL,THREAD_PRIORITY_LOWEST};
    return priorities[((value.priority-256)*5)/512];
}
inline int thread_create_domain_control() {
    std::uint64_t cell=0;thread_attributes_init(&cell);
    {std::lock_guard<std::mutex> guard(thread_attributes_lock);auto& value=*thread_attributes.at(cell);value.inherit=0;value.stack_size=32768;}
    const auto snapshot=snapshot_create_attributes(&cell);validate_startup_create_domain(snapshot);
    {std::lock_guard<std::mutex> guard(thread_attributes_lock);thread_attributes.at(cell)->priority=701;}
    if(snapshot.priority!=700 || snapshot.stack_size!=32768)return 1;
    unsigned rejected=0;
    for(unsigned mode=0;mode<10;++mode) {
        auto bad=snapshot;
        switch(mode){case 0:bad.inherit=4;break;case 1:bad.policy=2;break;case 2:bad.priority=768;break;case 3:bad.suspended=true;break;case 4:bad.detached=true;break;case 5:bad.system_scope=false;break;case 6:bad.stack_address=1;break;case 7:bad.cpu_set_size=8;bad.cpu_set=0;break;case 8:bad.cpu_set_size=8;break;case 9:bad.stack_size=0;break;}
        try{validate_startup_create_domain(bad);}catch(const std::runtime_error&){++rejected;}
    }
    std::uint64_t unknown=1;try{snapshot_create_attributes(&unknown);}catch(const std::runtime_error&){++rejected;}
    {std::lock_guard<std::mutex> guard(thread_attributes_lock);thread_attributes.erase(cell);}
    if(rejected!=10 || startup_windows_priority(snapshot)!=THREAD_PRIORITY_LOWEST)return 1;
    auto observed=snapshot;observed.priority=478;
    if(startup_windows_priority(observed)!=THREAD_PRIORITY_NORMAL)return 1;
    for(int priority=256;priority<=767;++priority){observed.priority=priority;const auto mapped=startup_windows_priority(observed);if(mapped<-2 || mapped>2)return 1;}
    std::cout<<"{\"scope\":\"startup_create_attribute_domain\",\"snapshot_stable\":true,\"unsupported_domains_rejected\":10,\"priority_values_checked\":512,\"observed478_maps_normal\":true,\"scheduler_equivalence\":false}\n";return 0;
}
}
