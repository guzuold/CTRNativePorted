// SPDX-License-Identifier: GPL-2.0-or-later
// Windows clock conversion adapted from shadPS4 kernel/time.cpp posix_clock_gettime
// revision e3ce810f3a653f43ac64ebab63023de281a4103a; clocks outside explicit cases missing.
#pragma once
#include "kernel_time.hpp"
extern "C" int* ctr_guest_errno();
namespace ctrport {
struct PosixTimespec {std::int64_t seconds,nanoseconds;};
inline int posix_clock(unsigned id,PosixTimespec* output) {
    if(!output){*ctr_guest_errno()=14;return -1;}
    PosixTimespec value{};
    switch(id) {
    case 0:case 9:case 10:case 13: {
        FILETIME time{};if(id==0 || id==9)GetSystemTimePreciseAsFileTime(&time);else GetSystemTimeAsFileTime(&time);
        const auto ticks=((static_cast<std::uint64_t>(time.dwHighDateTime)<<32)|time.dwLowDateTime)-116444736000000000ULL;
        value.seconds=ticks/10000000;value.nanoseconds=(ticks%10000000)*100;break;
    }
    case 4:case 5:case 7:case 8:case 11:case 12: {
        LARGE_INTEGER ticks{},frequency{};
        if(!QueryPerformanceFrequency(&frequency) || frequency.QuadPart<=0 || !QueryPerformanceCounter(&ticks)){*ctr_guest_errno()=14;return -1;}
        value.seconds=ticks.QuadPart/frequency.QuadPart;value.nanoseconds=(ticks.QuadPart%frequency.QuadPart)*1000000000/frequency.QuadPart;break;
    }
    default:throw std::runtime_error("Clock id unsupported; no clock substituted");
    }
    require_time_output(output,sizeof(*output));*output=value;return 0;
}
}
