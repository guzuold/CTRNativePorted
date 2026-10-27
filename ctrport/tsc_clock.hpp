// SPDX-License-Identifier: GPL-2.0-only
// Independent calibration; public behavior reference shadPS4 native_clock.cpp,
// rdtsc.cpp and kernel/time.cpp, e3ce810f3a653f43ac64ebab63023de281a4103a,
// GPL-2.0-or-later. Original guest RDTSC remains native CPU RDTSC.
#pragma once
#include "kernel_time.hpp"
#include <intrin.h>
#include <algorithm>
#include <array>
namespace ctrport {
inline std::uint64_t fenced_tsc() {
    _mm_lfence();
    const auto v = __rdtsc();
    _mm_lfence();
    return v;
}
inline std::uint64_t measure_tsc(unsigned milliseconds) {
    LARGE_INTEGER frequency{}, begin{}, end{};
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0)
        throw std::runtime_error("QPC reference unavailable");
    QueryPerformanceCounter(&begin);
    const auto first = fenced_tsc();
    do {
        Sleep(1);
        QueryPerformanceCounter(&end);
    } while (end.QuadPart - begin.QuadPart < frequency.QuadPart * milliseconds / 1000);
    const auto last = fenced_tsc();
    if (last <= first) throw std::runtime_error("Native TSC not increasing");
    return static_cast<std::uint64_t>(static_cast<long double>(last - first) * frequency.QuadPart /
                                      (end.QuadPart - begin.QuadPart));
}
inline std::uint64_t native_tsc_frequency() {
    static const auto frequency = [] {
        int cpu[4]{};
        __cpuid(cpu, static_cast<int>(0x80000000U));
        if (static_cast<unsigned>(cpu[0]) < 0x80000007U) throw std::runtime_error("Invariant TSC capability missing");
        __cpuid(cpu, static_cast<int>(0x80000007U));
        if (!(cpu[3] & (1 << 8))) throw std::runtime_error("Invariant TSC required");
        std::array<std::uint64_t, 3> samples{measure_tsc(50), measure_tsc(50), measure_tsc(50)};
        std::sort(samples.begin(), samples.end());
        if (!samples[1] || samples[2] - samples[0] > samples[1] / 100)
            throw std::runtime_error("Native TSC calibration unstable");
        return samples[1];
    }();
    return frequency;
}
}
