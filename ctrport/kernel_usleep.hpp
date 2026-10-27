// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "kernel_time.hpp"
namespace ctrport {
// New Windows implementation. Public usleep u32->nanosleep duration contract:
// shadPS4 time.cpp e3ce810f3a653f43ac64ebab63023de281a4103a GPL-2.0-or-later.
// Real non-alertable host wait; signal interruption unsupported by this runtime.
inline int kernel_usleep(uint32_t microseconds){
 if(!microseconds){SwitchToThread();return 0;}
 HANDLE timer=CreateWaitableTimerExW(nullptr,nullptr,0x2,TIMER_ALL_ACCESS);
 if(!timer)throw std::runtime_error("Usleep high resolution waitable timer unavailable");
 LARGE_INTEGER due{};due.QuadPart=-static_cast<LONGLONG>(microseconds)*10;
 if(!SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE)){CloseHandle(timer);throw std::runtime_error("Usleep timer arm failed");}
 const auto waited=WaitForSingleObject(timer,INFINITE);CloseHandle(timer);
 if(waited!=WAIT_OBJECT_0)throw std::runtime_error("Usleep real timer wait failed");return 0;
}
inline std::atomic<uint64_t> usleep_calls{0};
extern "C" int ctr_usleep(uint32_t microseconds,uint64_t caller){
 try{const auto result=kernel_usleep(microseconds);const auto count=usleep_calls.fetch_add(1)+1;
  if(count && !(count&(count-1))){std::printf("{\"scope\":\"original_Usleep_sample\",\"caller\":%llu,\"microseconds\":%u,\"count\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),microseconds,static_cast<unsigned long long>(count),result);std::fflush(stdout);}return result;
 }catch(const std::exception& error){std::fprintf(stderr,"NID=1jfXLRVzisc library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_usleep();
extern "C" int ctr_usleep_bridge_fixture(uint32_t);
}
