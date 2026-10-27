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
}
