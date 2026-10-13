// SPDX-License-Identifier: GPL-2.0-only
// New callback registration/query implementation, ABI observed in retail libc.
// Evidence: (development notes)
#pragma once
#include "thread_destructor.hpp"
namespace ctrport {
class ThreadAtexitRegistry {
    std::mutex lock;void* callback=nullptr;
public:
    void set(void* value) {
        if(value) {
            MEMORY_BASIC_INFORMATION info{};
            if(!VirtualQuery(value,&info,sizeof(info)) || info.State!=MEM_COMMIT ||
               (info.Protect!=PAGE_EXECUTE_READ && info.Protect!=PAGE_EXECUTE_READWRITE))
                throw std::runtime_error("Thread atexit callback not executable");
        }
        std::lock_guard<std::mutex> guard(lock);callback=value;
    }
    void report(int discriminator) {
        void* value=nullptr;{std::lock_guard<std::mutex> guard(lock);value=callback;}
        if(!value) throw std::runtime_error("Thread atexit report callback unavailable");
        (void)ctr_call_original_integer3(value,static_cast<unsigned>(discriminator),0,0);
    }
    int query(int discriminator) {
        void* value=nullptr;{std::lock_guard<std::mutex> guard(lock);value=callback;}
        if(!value) throw std::runtime_error("Thread atexit query callback unavailable");
        return static_cast<int>(ctr_call_original_integer3(value,static_cast<unsigned>(discriminator),0,0));
    }
};
inline ThreadAtexitRegistry thread_atexit;
inline ThreadAtexitRegistry thread_atexit_report;
extern "C" void ctr_sysv_atexit_report_control();
inline std::atomic<int> report_discriminator{0};
extern "C" void ctr_sysv_atexit_control();
inline int thread_atexit_report_control() {
    ThreadAtexitRegistry state;unsigned rejected=0;
    try {state.report(7);} catch(const std::runtime_error&) {++rejected;}
    unsigned data=0;
    try {state.set(&data);} catch(const std::runtime_error&) {++rejected;}
    state.set(reinterpret_cast<void*>(&ctr_sysv_atexit_report_control));
    for(int input:{7,-1}) {state.report(input);if(report_discriminator!=input) throw std::runtime_error("Report input routing differs");}
    state.set(nullptr);
    try {state.report(0);} catch(const std::runtime_error&) {++rejected;}
    if(rejected!=3) throw std::runtime_error("Report negatives differ");
    std::cout<<"{\"scope\":\"synthetic_atexit_report_callback\",\"invocations\":2,\"negative_rejections\":3,\"guest_scheduler_supported\":false}\n";
    return 0;
}
inline int thread_atexit_control() {
    ThreadAtexitRegistry state;unsigned rejected=0;
    try {state.query(0);} catch(const std::runtime_error&) {++rejected;}
    try {state.set(reinterpret_cast<void*>(1));} catch(const std::runtime_error&) {++rejected;}
    state.set(reinterpret_cast<void*>(&ctr_sysv_atexit_control));
    for(int input:{0,7,-1}) if(state.query(input)!=input) throw std::runtime_error("Callback discriminator/result routing differs");
    state.set(nullptr);
    try {state.query(7);} catch(const std::runtime_error&) {++rejected;}
    if(rejected!=3) throw std::runtime_error("Atexit negatives differ");
    std::cout<<"{\"scope\":\"synthetic_atexit_callback_query\",\"queries\":3,\"negative_rejections\":3,\"guest_scheduler_supported\":false}\n";
    return 0;
}
}
