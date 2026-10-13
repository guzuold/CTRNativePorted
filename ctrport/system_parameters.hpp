// SPDX-License-Identifier: GPL-2.0-only
// Own virtual-console configuration; public param ABI from shadPS4 systemservice.h/cpp
// e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later.
#pragma once
#include "kernel_time.hpp"
#include <cstdlib>
#include <array>
#include <climits>
#include "original_call.hpp"
namespace ctrport {
inline int system_parameter(int id,int* output) {
    if(!output)return static_cast<int>(0x80a10003U);
    int value=0;
    if(id==1) {
        // Deliberate runtime default US English; no host/emulator profile copied.
        value=1;
        char setting[32]{};auto length=GetEnvironmentVariableA("CTR_SYSTEM_LANGUAGE",setting,32);
        if(length>=32)throw std::runtime_error("CTR_SYSTEM_LANGUAGE too long");
        if(length) {
            char* end=nullptr;auto parsed=std::strtol(setting,&end,10);
            // Initially support the public pre-firmware language domain only.
            if(!*setting || *end || parsed<0 || parsed>18)throw std::runtime_error("CTR_SYSTEM_LANGUAGE must be integer0..18");
            value=static_cast<int>(parsed);
        }
    } else if(id==1000) {
        // Runtime-owned virtual-console preference: Cross=1, Circle=0.
        // Public enum OrbisSystemParamEnterButtonAssign, not a fabricated
        // controller/query result. No host or emulator profile imported.
        value=1;char setting[16]{};const auto length=GetEnvironmentVariableA("CTR_ENTER_BUTTON",setting,16);
        if(length>=16)throw std::runtime_error("CTR_ENTER_BUTTON too long");
        if(length){if(std::strcmp(setting,"0") && std::strcmp(setting,"1"))throw std::runtime_error("CTR_ENTER_BUTTON must be0(Circle) or1(Cross)");value=setting[0]-'0';}
    } else throw std::runtime_error("System parameter not configured");
    require_time_output(output,4);*output=value;return 0;
}
// Public systemservice.h86 and systemservice.cpp1777: status prefix ABI.
// Runtime-owned foreground execution policy: no shell overlay, broadcast or VR
// session exists. CPU virtual affinity domain is seven cores. Host focus does
// not suspend guest execution. Events are counted from the owned queue only.
inline std::mutex system_event_lock;
inline std::vector<std::uint32_t> system_pending_events;
inline int system_status(void* output){
    if(!output)return static_cast<int>(0x80a10003U);
    require_time_output(output,9);
    std::lock_guard<std::mutex> guard(system_event_lock);
    if(system_pending_events.size()>static_cast<std::size_t>(INT_MAX))throw std::runtime_error("System event count overflow");
    const auto count=static_cast<std::int32_t>(system_pending_events.size());
    auto* bytes=static_cast<unsigned char*>(output);
    std::memcpy(bytes,&count,4);bytes[4]=0;bytes[5]=0;bytes[6]=1;bytes[7]=0;bytes[8]=0;
    return 0;
}
extern "C" int ctr_system_status(void* output,std::uint64_t caller){
    try{const int result=system_status(output);std::printf("{\"scope\":\"owned_system_status_return\",\"caller\":%llu,\"result\":%d}\n",static_cast<unsigned long long>(caller),result);std::fflush(stdout);return result;}
    catch(const std::exception& e){std::fprintf(stderr,"NID=rPo6tV8D9bM library=libSceSystemService caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_system_status();
inline int system_status_control(){
    std::array<unsigned char,16> output;output.fill(0xa5);
    if(ctr_call_original_integer2(reinterpret_cast<void*>(&ctr_sysv_system_status),reinterpret_cast<std::uint64_t>(output.data()),0))return 1;
    std::int32_t count=-1;std::memcpy(&count,output.data(),4);
    if(count || output[4] || output[5] || output[6]!=1 || output[7] || output[8])return 1;
    for(unsigned i=9;i<16;++i)if(output[i]!=0xa5)return 1;
    {std::lock_guard<std::mutex> guard(system_event_lock);system_pending_events={7,9};}
    if(system_status(output.data()))return 1;
    std::memcpy(&count,output.data(),4);
    if(count!=2 || system_pending_events.size()!=2 || system_status(nullptr)!=static_cast<int>(0x80a10003U))return 1;
    system_pending_events.clear();
    std::cout<<"{\"scope\":\"system_status_control\",\"SysV_output_prefix\":true,\"reserved_preserved\":true,\"authored_pending_events\":2,\"query_does_not_consume\":true,\"runtime_events_fabricated\":false}\n";return 0;
}

}
