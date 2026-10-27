// SPDX-License-Identifier: GPL-2.0-only
// New callback registration/query implementation, ABI observed in retail libc.
#pragma once
#include "thread_destructor.hpp"
namespace ctrport {
class ThreadAtexitRegistry {
    std::mutex lock;
    void* callback = nullptr;

public:
    void set(void* value) {
        if (value) {
            MEMORY_BASIC_INFORMATION info{};
            if (!VirtualQuery(value, &info, sizeof(info)) || info.State != MEM_COMMIT ||
                (info.Protect != PAGE_EXECUTE_READ && info.Protect != PAGE_EXECUTE_READWRITE))
                throw std::runtime_error("Thread atexit callback not executable");
        }
        std::lock_guard<std::mutex> guard(lock);
        callback = value;
    }
    void report(int discriminator) {
        void* value = nullptr;
        {
            std::lock_guard<std::mutex> guard(lock);
            value = callback;
        }
        if (!value) throw std::runtime_error("Thread atexit report callback unavailable");
        (void)ctr_call_original_integer3(value, static_cast<unsigned>(discriminator), 0, 0);
    }
    int query(int discriminator) {
        void* value = nullptr;
        {
            std::lock_guard<std::mutex> guard(lock);
            value = callback;
        }
        if (!value) throw std::runtime_error("Thread atexit query callback unavailable");
        return static_cast<int>(ctr_call_original_integer3(value, static_cast<unsigned>(discriminator), 0, 0));
    }
};
inline ThreadAtexitRegistry thread_atexit;
inline ThreadAtexitRegistry thread_atexit_report;
extern "C" void ctr_sysv_atexit_report_control();
inline std::atomic<int> report_discriminator{0};
extern "C" void ctr_sysv_atexit_control();
}
