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
inline int gnm_flip_abi_control(){
 const std::array<uint64_t,9> values{3,0xfedcba9876543210ULL,0x123456789abcdef0ULL,0xabcdef0123456780ULL,0x1020304050607080ULL,0x80000001,0xdeadbeef,0x87654321,0xfedcba9876543210ULL};
 flip_abi_control_active=true;const int result=ctr_gnm_flip_fixture(values.data());flip_abi_control_active=false;
 if(result!=42 || !std::equal(values.begin(),values.end(),flip_abi_capture.begin()) || !flip_abi_capture[9])return 1;
 std::cout<<"{\"scope\":\"SubmitAndFlip_SysV9_transport_control\",\"passed\":true,\"stack_arguments\":3,\"arg64_preserved\":true,\"synthetic\":true,\"runtime_bound\":false,\"gpu_execution\":false}\n";return 0;
}
}
