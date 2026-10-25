// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "../renderer_controls/live.hpp"
#include <cstdio>
#include <chrono>
#include <thread>
#include <mutex>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <cstdint>
namespace ctrport::settings::pacing {
inline unsigned limit(const char* p){if(!p||!std::strcmp(p,"original"))return 0;for(unsigned n:{30u,60u,90u,120u})if(std::to_string(n)==p)return n;throw std::invalid_argument("invalid presentation limit");}
// Absolute monotonic deadlines preserve phase under small timer jitter. A full
// missed period rebases instead of issuing catch-up bursts. No game clock touched.
struct Timeline {
 std::int64_t next=0;bool started=false;
 std::int64_t reserve(std::int64_t now,std::int64_t period){
  if(period<=0)return now;
  if(!started||now>=next+period){started=true;next=now;}
  const auto due=next>now?next:now;next+=period;return due;
 }
};
inline void wait_before_present(){
 static const auto startup_fps=limit(std::getenv("CTR_SETTINGS_PRESENTATION_LIMIT"));
 const auto word=live_gpu::presentation.word.load(std::memory_order_acquire);
 const auto fps=word>>32 ? live_gpu::cap(static_cast<unsigned>(word)) : startup_fps;
 using clock=std::chrono::steady_clock;using ns=std::chrono::nanoseconds;
 static std::mutex mutex;static Timeline timeline;
 const auto now=std::chrono::duration_cast<ns>(clock::now().time_since_epoch()).count();
 std::int64_t due;
 {std::lock_guard<std::mutex> lock(mutex);
  static std::uint64_t observed=0;
  if(observed!=word){observed=word;timeline={};std::printf("settings_live_consumed key=presentation_limit revision=%llu cap=%u\n",static_cast<unsigned long long>(word>>32),fps);std::fflush(stdout);}
  if(!fps){timeline={};return;}
  due=timeline.reserve(now,(1000000000LL+fps-1)/fps);
 }
 std::this_thread::sleep_until(clock::time_point(ns(due)));
}
}
