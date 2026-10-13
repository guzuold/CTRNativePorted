// SPDX-License-Identifier: GPL-2.0-or-later
// Windows conversion adapted from shadPS4 v0.18.0 kernel/time.cpp
// e3ce810f3a653f43ac64ebab63023de281a4103a, posix_gettimeofday.
#pragma once
#include <Windows.h>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <atomic>
#include <cstdio>
#include <map>
#include <vector>
#include <iostream>
#include <stdexcept>
namespace ctrport {
struct KernelTimeval { std::int64_t seconds,microseconds; };
struct KernelTimezone { std::int32_t minutes_west,dst; };
static_assert(sizeof(KernelTimeval)==16 && sizeof(KernelTimezone)==8);
// Direct-memory owners publish only successful PAGE_READWRITE commits.
// Unmap/protection changes are unsupported; future implementations must revoke
// these extents before changing native mappings. This is ownership, not a query cache.
inline std::mutex owned_output_lock;
inline std::map<const void*,std::vector<std::pair<std::uintptr_t,std::uintptr_t>>> owned_output_extents;
inline void publish_owned_output(const void* owner,const void* pointer,std::size_t bytes){
    const auto start=reinterpret_cast<std::uintptr_t>(pointer);
    if(!owner || !bytes || start+bytes<start)throw std::runtime_error("Owned output extent invalid");
    std::lock_guard<std::mutex> guard(owned_output_lock);
    owned_output_extents[owner].emplace_back(start,start+bytes);
}
inline void revoke_owned_output(const void* owner){
    std::lock_guard<std::mutex> guard(owned_output_lock);owned_output_extents.erase(owner);
}
inline bool is_owned_output(std::uintptr_t start,std::uintptr_t end){
    std::lock_guard<std::mutex> guard(owned_output_lock);
    for(const auto& owner:owned_output_extents)for(const auto& extent:owner.second)
        if(start>=extent.first && end<=extent.second)return true;
    return false;
}
inline std::atomic<std::uint64_t> output_query_observations{0};
inline void require_time_output(const void* pointer,std::size_t size) {
    if(!pointer) return;
    auto address=reinterpret_cast<std::uintptr_t>(pointer);
    if(address+size<address) throw std::runtime_error("Time output address overflows");
    const auto end=address+size;
    if(size && is_owned_output(address,end))return;
    while(address<end) {
        const auto count=output_query_observations.fetch_add(1)+1;
        if(count && !(count&(count-1))){
            std::fprintf(stderr,"{\"scope\":\"output_query_sample\",\"count\":%llu,\"address\":%llu,\"size\":%llu,\"thread\":%lu}\n",static_cast<unsigned long long>(count),static_cast<unsigned long long>(address),static_cast<unsigned long long>(size),GetCurrentThreadId());std::fflush(stderr);
        }
        MEMORY_BASIC_INFORMATION info{};
        const bool sampled=count && !(count&(count-1));
        LARGE_INTEGER before{},after{},frequency{};
        if(sampled)QueryPerformanceCounter(&before);
        const auto queried=VirtualQuery(reinterpret_cast<const void*>(address),&info,sizeof(info));
        if(sampled){
            QueryPerformanceCounter(&after);QueryPerformanceFrequency(&frequency);
            std::fprintf(stderr,"{\"scope\":\"output_query_latency\",\"count\":%llu,\"address\":%llu,\"region_size\":%llu,\"query_bytes\":%llu,\"ticks\":%lld,\"frequency\":%lld,\"thread\":%lu}\n",static_cast<unsigned long long>(count),static_cast<unsigned long long>(address),static_cast<unsigned long long>(info.RegionSize),static_cast<unsigned long long>(queried),static_cast<long long>(after.QuadPart-before.QuadPart),static_cast<long long>(frequency.QuadPart),GetCurrentThreadId());std::fflush(stderr);
        }
        if(!queried || info.State!=MEM_COMMIT || (info.Protect&PAGE_GUARD)) throw std::runtime_error("Time output is not committed writable memory");
        const auto protection=info.Protect&0xff;
        if(protection!=PAGE_READWRITE && protection!=PAGE_WRITECOPY && protection!=PAGE_EXECUTE_READWRITE && protection!=PAGE_EXECUTE_WRITECOPY) throw std::runtime_error("Time output is not writable");
        const auto next=reinterpret_cast<std::uintptr_t>(info.BaseAddress)+info.RegionSize;
        if(next<=address) throw std::runtime_error("Time output region overflows");
        address=next;
    }
}
inline std::uint64_t unix_microseconds() {
    FILETIME time{};GetSystemTimePreciseAsFileTime(&time);
    const auto ticks=(static_cast<std::uint64_t>(time.dwHighDateTime)<<32)|time.dwLowDateTime;
    return ticks/10-11644473600000000ULL;
}
inline int kernel_gettimeofday(KernelTimeval* value,KernelTimezone* zone) {
    require_time_output(value,sizeof(KernelTimeval));require_time_output(zone,sizeof(KernelTimezone));
    if(value) {
        const auto micros=unix_microseconds();
        value->seconds=static_cast<std::int64_t>(micros/1000000);
        value->microseconds=static_cast<std::int64_t>(micros%1000000);
    }
    if(zone) {
        static std::once_flag initialized;
        std::call_once(initialized,[]{_tzset();});
        long seconds_west=0;int daylight=0;
        if(_get_timezone(&seconds_west) || _get_daylight(&daylight))
            throw std::runtime_error("Kernel timezone CRT query failed");
        zone->minutes_west=static_cast<std::int32_t>(seconds_west/60);
        zone->dst=daylight;
    }
    return 0;
}
inline int kernel_time_control() {
    struct Framed { std::uint64_t before; KernelTimeval time; std::uint64_t after; };
    Framed frame{0x1357,{-1,-1},0x2468};KernelTimezone zone{};
    const auto low=unix_microseconds();
    if(kernel_gettimeofday(&frame.time,&zone)) throw std::runtime_error("Kernel clock returned error");
    const auto high=unix_microseconds();
    const auto actual=static_cast<std::uint64_t>(frame.time.seconds)*1000000+static_cast<std::uint64_t>(frame.time.microseconds);
    if(actual<low || actual>high || frame.time.microseconds<0 || frame.time.microseconds>=1000000 || frame.before!=0x1357 || frame.after!=0x2468)
        throw std::runtime_error("Kernel clock bounds/layout differ");
    if(kernel_gettimeofday(nullptr,&zone) || kernel_gettimeofday(nullptr,nullptr)) throw std::runtime_error("Kernel optional argument contract differs");
    std::cout<<"{\"scope\":\"host_kernel_gettimeofday_contract\",\"clock_bounds\":true,\"layout_sentinels\":true,\"optional_arguments\":true,\"retail_executed\":false,\"runtime_bound\":false}\n";
    return 0;
}
}
