// SPDX-License-Identifier: GPL-2.0-only
// Independent owned attribute storage. Contract: pinned shadPS4 pthread_attr.cpp
// defaults and posix_pthread_attr_init; no thread or scheduler service supplied.
#pragma once
#include "kernel_mutex.hpp"
namespace ctrport {
struct ThreadAttributes {
    int policy=1,inherit=4,priority=700;
    bool suspended=false,system_scope=true,detached=false;
    std::uint64_t stack_address=0,stack_size=1048576,guard_size=0,cpu_set_size=0,cpu_set=0;
};
inline std::mutex thread_attributes_lock;
inline std::unordered_map<std::uint64_t,std::shared_ptr<ThreadAttributes>> thread_attributes;
inline int thread_attributes_init(std::uint64_t* cell) {
    require_time_output(cell,8);
    auto object=std::make_shared<ThreadAttributes>();
    auto handle=reinterpret_cast<std::uint64_t>(object.get());
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    thread_attributes.emplace(handle,object);*cell=handle;return 0;
}
inline int thread_attributes_affinity(std::uint64_t* cell,std::uint64_t* output) {
    require_time_output(cell,8);require_time_output(output,8);
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    const auto found=thread_attributes.find(*cell);
    if(found==thread_attributes.end()) return static_cast<int>(0x80020016U);

    // Null attribute cpuset means unrestricted, not the Windows processor mask.
    *output=found->second->cpu_set_size?found->second->cpu_set:~std::uint64_t{0};return 0;
}

// Public pthread_attr.cpp283-319, e3ce810f3a653f43ac64ebab63023de281a4103a
// GPL-2.0-or-later contract; independent mask storage, no upstream code copied.
inline int thread_attributes_set_affinity(std::uint64_t* cell,std::uint64_t mask) {
    if(!cell)return int(0x80020016U);
    require_time_output(cell,8);
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    const auto found=thread_attributes.find(*cell);
    if(found==thread_attributes.end())return int(0x80020016U);
    found->second->cpu_set=mask;found->second->cpu_set_size=8;return 0;
}
extern "C" int ctr_attr_set_affinity(std::uint64_t* cell,std::uint64_t mask,std::uint64_t caller){
    try{return thread_attributes_set_affinity(cell,mask);}catch(const std::exception& error){std::fprintf(stderr,"NID=3qxgM4ezETA library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_attr_set_affinity();
inline int thread_attributes_destroy(std::uint64_t* cell);
inline int attr_set_affinity_control(){
    std::uint64_t cell=0,out=0;thread_attributes_init(&cell);
    for(auto mask:{std::uint64_t(63),std::uint64_t(0),~std::uint64_t(0)})if(thread_attributes_set_affinity(&cell,mask) || thread_attributes_affinity(&cell,&out) || out!=mask)return 1;
    std::uint64_t invalid=1;if(!thread_attributes_set_affinity(nullptr,63) || !thread_attributes_set_affinity(&invalid,63))return 1;
    if(thread_attributes_affinity(&cell,&out) || out!=~std::uint64_t(0) || thread_attributes_destroy(&cell))return 1;
    std::cout<<"{\"scope\":\"owned_attribute_affinity\",\"mask_readbacks\":3,\"invalid_handle_no_mutation\":true}\n";return 0;
}
// Independent detach-state storage: public shadPS4 pthread_attr.cpp197-206,
// e3ce810f3a653f43ac64ebab63023de281a4103a (GPL-2.0-or-later).
inline int thread_attributes_set_detach(std::uint64_t* cell,int state){
    if(!cell || (state!=0 && state!=1))return int(0x80020016U);
    require_time_output(cell,8);
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    const auto found=thread_attributes.find(*cell);
    if(found==thread_attributes.end())return int(0x80020016U);
    found->second->detached=state!=0;return 0;
}
extern "C" int ctr_attr_set_detach(std::uint64_t* cell,int state,std::uint64_t caller){
    try{return thread_attributes_set_detach(cell,state);}catch(const std::exception& error){std::fprintf(stderr,"NID=-Wreprtu0Qs library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_attr_set_detach();
inline int attr_set_detach_control(){
    std::uint64_t cell=0;thread_attributes_init(&cell);
    for(int state:{1,0,1}){if(thread_attributes_set_detach(&cell,state))return 1;std::lock_guard<std::mutex> guard(thread_attributes_lock);if(thread_attributes.at(cell)->detached!=(state!=0))return 1;}
    std::uint64_t invalid=1;
    if(!thread_attributes_set_detach(nullptr,0) || !thread_attributes_set_detach(&invalid,0) || !thread_attributes_set_detach(&cell,2) || !thread_attributes_set_detach(&cell,-1))return 1;
    {std::lock_guard<std::mutex> guard(thread_attributes_lock);if(!thread_attributes.at(cell)->detached)return 1;}
    if(thread_attributes_set_detach(&cell,0) || thread_attributes_destroy(&cell))return 1;
    std::cout<<"{\"scope\":\"owned_detach_attributes\",\"readbacks\":3,\"negative_cases\":4,\"negative_no_mutation\":true,\"detached_creation_supported\":false}\n";return 0;
}
// Attribute storage only: pinned pthread_attr.cpp143-151, priorities20-24.
inline int thread_attributes_set_policy(std::uint64_t* cell,int policy) {
    if(!cell) return int(0x80020016U);
    require_time_output(cell,8);
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    const auto found=thread_attributes.find(*cell);
    if(found==thread_attributes.end()) return int(0x80020016U);
    if(policy<1 || policy>3) return int(0x8002002dU);
    found->second->policy=policy;found->second->priority=policy==2?900:700;return 0;
}
inline int thread_policy_control() {
    std::uint64_t cell=0;thread_attributes_init(&cell);
    for(int policy:{2,3,1}) {
        if(thread_attributes_set_policy(&cell,policy)) throw std::runtime_error("Policy storage failed");
        std::lock_guard<std::mutex> guard(thread_attributes_lock);
        const auto& attr=*thread_attributes.at(cell);
        if(attr.policy!=policy || attr.priority!=(policy==2?900:700)) throw std::runtime_error("Policy defaults differ");
    }
    std::uint64_t invalid=1;
    if(thread_attributes_set_policy(&cell,0)!=int(0x8002002dU) || thread_attributes_set_policy(&cell,4)!=int(0x8002002dU) || thread_attributes_set_policy(&invalid,1)!=int(0x80020016U) || thread_attributes_set_policy(nullptr,1)!=int(0x80020016U)) throw std::runtime_error("Policy negatives differ");
    {std::lock_guard<std::mutex> guard(thread_attributes_lock);if(thread_attributes.at(cell)->policy!=1 || thread_attributes.at(cell)->priority!=700) throw std::runtime_error("Invalid policy mutated state");thread_attributes.erase(cell);}
    std::cout<<"{\"scope\":\"thread_policy_attributes\",\"policies_checked\":3,\"negative_cases\":4,\"scheduler_implemented\":false}\n";return 0;
}
// Public pthread_attr.cpp209-225; storage, not scheduler execution.
inline int thread_attributes_set_priority(std::uint64_t* cell,const int* parameter) {
    if(!cell) return int(0x80020016U);
    require_time_output(cell,8);
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    const auto found=thread_attributes.find(*cell);
    if(found==thread_attributes.end()) return int(0x80020016U);
    if(!parameter) return int(0x8002002dU);
    MEMORY_BASIC_INFORMATION region{};
    if(!VirtualQuery(parameter,&region,sizeof(region)) || region.State!=MEM_COMMIT || (region.Protect&(PAGE_GUARD|PAGE_NOACCESS)) || reinterpret_cast<std::uintptr_t>(parameter)+sizeof(int)>reinterpret_cast<std::uintptr_t>(region.BaseAddress)+region.RegionSize) throw std::runtime_error("Priority parameter unreadable");
    const int priority=*parameter;
    if((found->second->policy==1 || found->second->policy==3) && (priority<256 || priority>767)) return int(0x8002002dU);
    found->second->priority=priority;return 0;
}
inline int thread_priority_control() {
    std::uint64_t cell=0;thread_attributes_init(&cell);
    for(int value:{256,700,767}) {if(thread_attributes_set_priority(&cell,&value)) throw std::runtime_error("Priority store failed");std::lock_guard<std::mutex> guard(thread_attributes_lock);if(thread_attributes.at(cell)->priority!=value) throw std::runtime_error("Priority readback failed");}
    for(int value:{255,768}) if(thread_attributes_set_priority(&cell,&value)!=int(0x8002002dU)) throw std::runtime_error("Invalid priority accepted");
    if(thread_attributes_set_priority(&cell,nullptr)!=int(0x8002002dU)) throw std::runtime_error("Null parameter accepted");
    {std::lock_guard<std::mutex> guard(thread_attributes_lock);if(thread_attributes.at(cell)->priority!=767) throw std::runtime_error("Rejected priority mutated state");thread_attributes.erase(cell);}
    std::cout<<"{\"scope\":\"thread_priority_attributes\",\"readbacks\":3,\"negative_cases\":3,\"scheduler_implemented\":false}\n";return 0;
}
// Public pthread_attr.cpp228-237: attribute storage only.
inline int thread_attributes_set_inherit(std::uint64_t* cell,int inherit) {
    if(!cell) return int(0x80020016U);
    require_time_output(cell,8);
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    const auto found=thread_attributes.find(*cell);
    if(found==thread_attributes.end()) return int(0x80020016U);
    if(inherit!=0 && inherit!=4) return int(0x8002002dU);
    found->second->inherit=inherit;return 0;
}
inline int thread_inherit_control() {
    std::uint64_t cell=0;thread_attributes_init(&cell);
    for(int value:{0,4}) {if(thread_attributes_set_inherit(&cell,value)) return 1;std::lock_guard<std::mutex> guard(thread_attributes_lock);if(thread_attributes.at(cell)->inherit!=value) return 1;}
    std::uint64_t unknown=1;
    if(thread_attributes_set_inherit(&cell,1)!=int(0x8002002dU) || thread_attributes_set_inherit(&unknown,0)!=int(0x80020016U) || thread_attributes_set_inherit(nullptr,0)!=int(0x80020016U)) return 1;
    {std::lock_guard<std::mutex> guard(thread_attributes_lock);if(thread_attributes.at(cell)->inherit!=4) return 1;thread_attributes.erase(cell);}
    std::cout<<"{\"scope\":\"thread_inherit_attributes\",\"readbacks\":2,\"negative_cases\":3,\"scheduler_implemented\":false}\n";return 0;
}
inline int thread_attributes_set_stack_size(std::uint64_t* cell,std::uint64_t size) {
    if(!cell || size<16384) return static_cast<int>(0x80020016U);
    require_time_output(cell,8);
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    const auto found=thread_attributes.find(*cell);
    if(found==thread_attributes.end()) return static_cast<int>(0x80020016U);
    found->second->stack_size=size;return 0;
}
inline int thread_stack_size_control() {
    std::uint64_t cell=0;thread_attributes_init(&cell);
    for(auto size:{std::uint64_t(16384),std::uint64_t(32768),std::uint64_t(32769)}) {
        if(thread_attributes_set_stack_size(&cell,size)) throw std::runtime_error("Stack size update failed");
        std::lock_guard<std::mutex> guard(thread_attributes_lock);
        if(thread_attributes.at(cell)->stack_size!=size) throw std::runtime_error("Stack size not stored");
    }
    std::uint64_t unknown=1;
    if(thread_attributes_set_stack_size(&cell,16383)!=int(0x80020016U) ||
       thread_attributes_set_stack_size(nullptr,32768)!=int(0x80020016U) ||
       thread_attributes_set_stack_size(&unknown,32768)!=int(0x80020016U)) throw std::runtime_error("Invalid stack size/handle accepted");
    {std::lock_guard<std::mutex> guard(thread_attributes_lock);if(thread_attributes.at(cell)->stack_size!=32769) throw std::runtime_error("Rejected stack size mutated state");}
    {std::lock_guard<std::mutex> guard(thread_attributes_lock);thread_attributes.erase(cell);}
    std::cout<<"{\"scope\":\"thread_stack_size_host\",\"stored_sizes\":3,\"rejected_cases\":3,\"negative_no_mutation\":true,\"thread_creation_supported\":false}\n";return 0;
}

inline int thread_attributes_destroy(std::uint64_t* cell) {
    require_time_output(cell,8);
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    const auto found=thread_attributes.find(*cell);
    if(found==thread_attributes.end()) return static_cast<int>(0x80020016U);
    thread_attributes.erase(found);*cell=0;return 0;
}

// Independent public pthread_attr.cpp99-107 contract; owned attribute snapshot.
inline int thread_attributes_get_stack(std::uint64_t* cell,std::uint64_t* address,std::uint64_t* size){
    if(!cell || !address || !size)return int(0x80020016U);
    require_time_output(cell,8);
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    const auto found=thread_attributes.find(*cell);
    if(found==thread_attributes.end())return int(0x80020016U);
    require_time_output(address,8);require_time_output(size,8);
    *address=found->second->stack_address;*size=found->second->stack_size;return 0;
}
extern "C" int ctr_attr_get_stack(std::uint64_t* cell,std::uint64_t* address,std::uint64_t* size,std::uint64_t caller){
    try{const int result=thread_attributes_get_stack(cell,address,size);
        std::printf("{\"scope\":\"owned_attribute_get_stack\",\"nid\":\"-quPa4SEJUw\",\"library\":\"libkernel\",\"caller\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),result);return result;
    }catch(const std::exception& e){std::fprintf(stderr,"NID=-quPa4SEJUw library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_attr_get_stack();

}
