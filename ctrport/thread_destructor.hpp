// SPDX-License-Identifier: GPL-2.0-only
// New scoped registration/lifecycle implementation, after the public shadPS4 v0.18.0 contract.
#pragma once
#include "original_call.hpp"
#include <mutex>
#include <thread>
#include <atomic>
namespace ctrport {
class ThreadDestructorRegistry {
    std::mutex lock;
    void* callback=nullptr;
public:
    void set(void* value) {
        if(value) {
            MEMORY_BASIC_INFORMATION info{};
            if(!VirtualQuery(value,&info,sizeof(info)) || info.State!=MEM_COMMIT ||
               (info.Protect!=PAGE_EXECUTE_READ && info.Protect!=PAGE_EXECUTE_READWRITE))
                throw std::runtime_error("Thread destructor callback not executable");
        }
        std::lock_guard<std::mutex> guard(lock);callback=value;
    }
    void on_owned_thread_exit() {
        void* value=nullptr;
        {std::lock_guard<std::mutex> guard(lock);value=callback;}
        if(value) (void)ctr_call_original_integer3(value,0,0,0);
    }
};
inline ThreadDestructorRegistry thread_destructors;
extern "C" void ctr_sysv_dtor_control();
extern "C" void ctr_dtor_control_increment();
inline std::atomic<unsigned> destructor_control_count{0};
inline int thread_destructor_control() {
    ThreadDestructorRegistry state;unsigned rejected=0;
    try {state.set(reinterpret_cast<void*>(1));} catch(const std::runtime_error&) {++rejected;}
    unsigned data=0;
    try {state.set(&data);} catch(const std::runtime_error&) {++rejected;}
    state.set(reinterpret_cast<void*>(&ctr_sysv_dtor_control));
    std::thread worker([&state]{state.on_owned_thread_exit();});worker.join();
    state.set(nullptr);state.on_owned_thread_exit();
    if(rejected!=2 || destructor_control_count!=1) throw std::runtime_error("Destructor lifecycle control differs");
    std::cout<<"{\"scope\":\"synthetic_owned_thread_destructor\",\"callback_count\":1,\"negative_rejections\":2,\"guest_threads_supported\":false}\n";
    return 0;
}
}
