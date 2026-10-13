// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <chrono>
#include <cstdio>
#include <string_view>
namespace ctrport {
// Opt-in host wall-time diagnostic. No shader or guest state is changed.
struct PairHostTiming {
 using Clock=std::chrono::steady_clock;
 bool enabled=false;const char* phase="resolve_path";Clock::time_point last{};
 PairHostTiming(){char flag[2]{};enabled=GetEnvironmentVariableA("CTR_TILING_TIMING",flag,sizeof(flag))==1 && flag[0]=='1';if(enabled)last=Clock::now();}
 void Next(const char* next){if(enabled){auto now=Clock::now();std::fprintf(stderr,"CTR_PAIR_HOST_PHASE phase=%s ms=%.6f\n",phase,std::chrono::duration<double,std::milli>(now-last).count());last=now;}phase=next;}
 ~PairHostTiming(){if(phase!=std::string_view("done"))Next("done");}
};
}
