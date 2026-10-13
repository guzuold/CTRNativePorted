// SPDX-License-Identifier: GPL-2.0-only
// Our Windows synchronization implementation of the observed default static
// mutex contract. Behavioral reference: pinned shadPS4 threads/mutex.cpp;
// source hashes and revision are in kernel_mutex_original_contract.json.
#pragma once
#include "kernel_time.hpp"
#include <condition_variable>
#include <memory>
#include <unordered_map>
#include <thread>
#include <future>
namespace ctrport {
struct KernelMutex {
    std::string name;
    std::mutex state;
    std::condition_variable available;
    std::thread::id owner;
    std::uint32_t type=1,depth=0;
};
inline std::mutex mutex_registry_lock;
inline std::unordered_map<std::uint64_t,std::shared_ptr<KernelMutex>> mutex_registry;
inline std::shared_ptr<KernelMutex> lookup_mutex(std::uint64_t* cell,bool initialize) {
    if(!cell) throw std::runtime_error("Mutex cell null unsupported");
    require_time_output(cell,8);
    std::lock_guard<std::mutex> guard(mutex_registry_lock);
    if(*cell==0 && initialize) {
        auto object=std::make_shared<KernelMutex>();
        const auto handle=reinterpret_cast<std::uint64_t>(object.get());
        mutex_registry.emplace(handle,object);*cell=handle;
    }
    const auto found=mutex_registry.find(*cell);
    if(found==mutex_registry.end()) throw std::runtime_error("Unknown/adaptive/destroyed mutex handle unsupported");
    return found->second;
}
inline int kernel_mutex_lock(std::uint64_t* cell) {
    auto object=lookup_mutex(cell,true);
    std::unique_lock<std::mutex> guard(object->state);
    const auto current=std::this_thread::get_id();
    if(object->owner==current) {
        if(object->type!=2) return 11; // PS4 POSIX_EDEADLK, default ErrorCheck.
        if(object->depth==0x80000000U) return 35; // PS4 POSIX_EAGAIN.
        ++object->depth;return 0;
    }
    object->available.wait(guard,[&]{return object->owner==std::thread::id{};});
    object->owner=current;object->depth=1;return 0;
}
inline int kernel_mutex_trylock(std::uint64_t* cell) {
    auto object=lookup_mutex(cell,true);
    std::unique_lock<std::mutex> guard(object->state,std::try_to_lock);if(!guard.owns_lock())return 16;
    const auto current=std::this_thread::get_id();
    if(object->owner==current) {
        if(object->type!=2)return 16;
        if(object->depth==0x80000000U)return 35;
        ++object->depth;return 0;
    }
    if(object->owner!=std::thread::id{})return 16;
    object->owner=current;object->depth=1;return 0;
}
inline int kernel_mutex_unlock(std::uint64_t* cell) {
    auto object=lookup_mutex(cell,false);
    std::lock_guard<std::mutex> guard(object->state);
    if(object->owner!=std::this_thread::get_id()) return 1; // PS4 POSIX_EPERM.
    if(--object->depth) return 0;
    object->owner=std::thread::id{};object->available.notify_one();return 0;
}
// Independent owned-object lifecycle; pinned mutex.cpp108-121,
// e3ce810f3a653f43ac64ebab63023de281a4103a GPL-2.0-or-later reference.
inline int kernel_mutex_destroy(std::uint64_t* cell){
 if(!cell)return static_cast<int>(0x80020016U);require_time_output(cell,8);
 std::lock_guard<std::mutex> guard(mutex_registry_lock);
 if(*cell==0)return 0;
 const auto found=mutex_registry.find(*cell);if(found==mutex_registry.end())return static_cast<int>(0x80020016U);
 if(found->second.use_count()!=1)return static_cast<int>(0x80020010U);
 {std::lock_guard<std::mutex> state(found->second->state);if(found->second->depth || found->second->owner!=std::thread::id{})return static_cast<int>(0x80020010U);}
 mutex_registry.erase(found);*cell=UINT64_MAX;return 0;
}
extern "C" int ctr_mutex_destroy(std::uint64_t* cell,std::uint64_t caller){
 try{const auto result=kernel_mutex_destroy(cell);std::printf("{\"scope\":\"original_MutexDestroy_return\",\"caller\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),result);std::fflush(stdout);return result;}
 catch(const std::exception& e){std::fprintf(stderr,"NID=2Of0f+3mhhE library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_mutex_destroy();
// POSIX variant (posix_pthread_mutex_destroy, shadPS4 mutex.cpp479/506): same object
// lifecycle, POSIX error numbers instead of 0x8002xxxx (EINVAL 22, EBUSY 16).
extern "C" int ctr_posix_mutex_destroy(std::uint64_t* cell,std::uint64_t caller){
 try{const auto orbis=kernel_mutex_destroy(cell);const int result=orbis?static_cast<int>(static_cast<unsigned>(orbis)&0xffffU):0;
  std::printf("{\"scope\":\"original_PosixMutexDestroy_return\",\"caller\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),result);std::fflush(stdout);return result;}
 catch(const std::exception& e){std::fprintf(stderr,"NID=ltCfaGr2JGE library=libScePosix caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_posix_mutex_destroy();
inline int mutex_destroy_control(){
 std::uint64_t cell=0;if(kernel_mutex_destroy(&cell) || kernel_mutex_lock(&cell))return 1;const auto handle=cell;
 if(kernel_mutex_destroy(&cell)!=static_cast<int>(0x80020010U) || cell!=handle || kernel_mutex_unlock(&cell))return 2;
 auto retained=lookup_mutex(&cell,false);if(kernel_mutex_destroy(&cell)!=static_cast<int>(0x80020010U))return 3;retained.reset();
 if(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_mutex_destroy),reinterpret_cast<std::uint64_t>(&cell),0,0) || cell!=UINT64_MAX || mutex_registry.count(handle))return 4;
 if(kernel_mutex_destroy(&cell)!=static_cast<int>(0x80020016U))return 5;
 std::cout<<"{\"scope\":\"owned_mutex_destroy\",\"passed\":true,\"locked_and_retained_busy\":true}\n";return 0;
}
inline int kernel_mutex_control() {
    std::uint64_t cell=0;
    if(kernel_mutex_lock(&cell) || !cell || kernel_mutex_lock(&cell)!=11) throw std::runtime_error("Mutex static/errorcheck contract differs");
    std::promise<void> attempting;
    auto worker=std::async(std::launch::async,[&]{
        if(kernel_mutex_unlock(&cell)!=1) throw std::runtime_error("Mutex foreign unlock accepted");
        attempting.set_value();
        if(kernel_mutex_lock(&cell) || kernel_mutex_unlock(&cell)) throw std::runtime_error("Mutex worker lock/unlock failed");
    });
    attempting.get_future().wait();
    const bool blocked=worker.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout;
    const auto unlock=kernel_mutex_unlock(&cell);
    worker.get();
    if(!blocked || unlock || kernel_mutex_unlock(&cell)!=1) throw std::runtime_error("Mutex contention/ownership differs");
    unsigned rejected=0;
    for(auto invalid:{std::uint64_t(1),std::uint64_t(2),std::uint64_t(0x12345678)}) {
        try { (void)kernel_mutex_lock(&invalid); } catch(const std::runtime_error&) {++rejected;}
    }
    if(rejected!=3) throw std::runtime_error("Unknown mutex handle accepted");
    std::cout<<"{\"scope\":\"host_default_static_mutex\",\"real_contention\":true,\"foreign_unlock_eperm\":true,\"relock_edeadlk\":true,\"unknown_handles_rejected\":3,\"runtime_bound\":false,\"retail_executed\":false}\n";
    return 0;
}
}
