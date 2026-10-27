// SPDX-License-Identifier: GPL-2.0-only
#pragma once
namespace ctrport {
// Transport preparation only. No public SubmitAndFlip binding until real executor exists.
inline thread_local bool flip_abi_control_active=false;
inline thread_local std::array<uint64_t,10> flip_abi_capture{};
extern "C" int ctr_gnm_flip_bridge(uint32_t count,uint64_t da,uint64_t ds,uint64_t ca,uint64_t cs,uint32_t vo,uint32_t buffer,uint32_t mode,int64_t arg,uint64_t caller){
 if(flip_abi_control_active){flip_abi_capture={count,da,ds,ca,cs,vo,buffer,mode,static_cast<uint64_t>(arg),caller};return 42;}
 std::fprintf(stderr,"NID=xbxNatawohc library=libSceGnmDriver caller=0x%llx unsupported: whole GPU execution and flip admission unavailable\n",static_cast<unsigned long long>(caller));std::fflush(stderr);ExitProcess(4);
}
extern "C" int ctr_gnm_flip_fixture(const uint64_t*);
}
