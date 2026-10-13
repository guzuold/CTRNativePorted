// SPDX-License-Identifier: GPL-2.0-only
// Own query implementation. ABI: FreeBSD releng/9.0 sys/sys/_sigset.h
// four32-bit words; retail libc whole caller0xb6860/77 stores at context+0x48.
// Non-null set requests fail explicitly until guest signal delivery is implemented.
#pragma once
#include <array>
#include "kernel_time.hpp"
namespace ctrport {
struct GuestSignalMask {std::array<std::uint32_t,4> blocked{};};
inline thread_local GuestSignalMask guest_signal_mask;
inline int query_signal_mask(int how,const void* set,void* old) {
    (void)how; // Ignored for null set, as required by the query contract.
    if(set)throw std::runtime_error("Guest signal mask mutation/delivery missing");
    if(old){require_time_output(old,sizeof(guest_signal_mask.blocked));std::memcpy(old,guest_signal_mask.blocked.data(),sizeof(guest_signal_mask.blocked));}
    return 0;
}
extern "C" int ctr_signal_mask(int how,const void* set,void* old,std::uint64_t caller) {
    try{return query_signal_mask(how,set,old);}catch(const std::exception& error){std::fprintf(stderr,"NID=6xVpy0Fdq+I library=libkernel caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),error.what());std::fflush(stderr);ExitProcess(4);}
}
extern "C" void ctr_sysv_signal_mask();
inline int signal_mask_query_control() {
    std::array<std::uint32_t,6> output{71,72,73,74,75,76};
    if(query_signal_mask(1,nullptr,output.data()+1) || output!=std::array<std::uint32_t,6>{71,0,0,0,0,76})return 1;
    if(query_signal_mask(99,nullptr,nullptr))return 1;
    // Inject a nonzero internal state only in this synthetic control, without claiming delivery.
    guest_signal_mask.blocked[2]=0x12345678;
    auto child=std::async(std::launch::async,[]{std::array<std::uint32_t,4> value{};return query_signal_mask(1,nullptr,value.data())==0 && value==std::array<std::uint32_t,4>{};});
    if(!child.get() || query_signal_mask(1,nullptr,output.data()+1) || output[3]!=0x12345678)return 1;
    guest_signal_mask.blocked={};unsigned rejected=0;
    try{query_signal_mask(3,output.data(),nullptr);}catch(const std::runtime_error&){++rejected;}
    auto memory=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READONLY);if(!memory)throw std::runtime_error("Signal mask control allocation failed");
    try{query_signal_mask(1,nullptr,memory);}catch(const std::runtime_error&){++rejected;}VirtualFree(memory,0,MEM_RELEASE);
    if(rejected!=2)return 1;
    std::cout<<"{\"scope\":\"guest_signal_mask_query\",\"bytes_written\":16,\"canaries_preserved\":true,\"thread_local_state_snapshot\":true,\"mutation_readonly_rejected\":2,\"signal_delivery_supported\":false}\n";return 0;
}
}
