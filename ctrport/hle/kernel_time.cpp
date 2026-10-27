// SPDX-License-Identifier: GPL-2.0-or-later
// libkernel nanosleep (first called by the game's libc in the online runtime, R8).
// Behaviour of pinned shadPS4 v0.18.0 src/core/libraries/kernel/time.cpp posix_nanosleep:
// sleeps for the requested time and returns 0, writing a zero remainder. The wait is a real
// high-resolution waitable timer, as ctrport/kernel_usleep.hpp; this runtime delivers no
// signals, so the sleep is never interrupted. An invalid timespec needs the guest errno
// (EINVAL), which ctr_hle cannot set: that case fails loudly instead of returning.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include "ctr_hle.h"

namespace {
struct Timespec {
    std::int64_t tv_sec;
    std::int64_t tv_nsec;
};
} // namespace

CTR_SYSV int nanosleep_(const Timespec* request, Timespec* remain) {
    if (!request || request->tv_sec < 0 || request->tv_nsec < 0 || request->tv_nsec >= 1000000000) {
        std::fprintf(stderr,
                     "NID=yS8U2TGCe1A library=libkernel caller=0x%llx unsupported: nanosleep "
                     "invalid timespec needs guest errno\n",
                     static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0))));
        std::fflush(stderr);
        ExitProcess(4);
    }
    const std::int64_t hundreds = request->tv_sec * 10000000 + (request->tv_nsec + 99) / 100;
    if (hundreds == 0) {
        SwitchToThread();
    } else {
        HANDLE timer =
            CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        LARGE_INTEGER due{};
        due.QuadPart = -hundreds;
        if (!timer || !SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE) ||
            WaitForSingleObject(timer, INFINITE) != WAIT_OBJECT_0) {
            std::fprintf(stderr, "NID=yS8U2TGCe1A library=libkernel unsupported: nanosleep "
                                 "host timer failed\n");
            std::fflush(stderr);
            ExitProcess(4);
        }
        CloseHandle(timer);
    }
    if (remain) {
        remain->tv_sec = 0;
        remain->tv_nsec = 0;
    }
    return 0;
}
CTR_HLE_REGISTER("yS8U2TGCe1A", "libkernel", nanosleep_);
