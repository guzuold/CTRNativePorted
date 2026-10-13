// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <chrono>
#include <cstdio>
namespace ctrport {
struct SnapshotPhaseTiming {
 using Clock=std::chrono::steady_clock;
 bool enabled=false;const char* operation;const char* phase="lock_wait";unsigned long long bytes;Clock::time_point last{};
 SnapshotPhaseTiming(const char* op,unsigned long long size):operation(op),bytes(size){char flag[2]{};enabled=GetEnvironmentVariableA("CTR_TILING_TIMING",flag,sizeof(flag))==1 && flag[0]=='1';if(enabled)last=Clock::now();}
 void Next(const char* next){if(enabled){auto now=Clock::now();std::fprintf(stderr,"CTR_SNAPSHOT_PHASE operation=%s bytes=%llu phase=%s ms=%.6f\n",operation,bytes,phase,std::chrono::duration<double,std::milli>(now-last).count());last=now;}phase=next;}
 ~SnapshotPhaseTiming(){Next("done");}
};
}
