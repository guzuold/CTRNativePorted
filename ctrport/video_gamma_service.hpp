// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "owned_guest_copy.hpp"
#include "video/guest_color_write.h"
namespace ctrport {
extern "C" int ctr_video_gamma(void* output,float gamma,std::uint64_t caller){
 const auto memory=make_owned_guest_copy_api();const int status=ctr_video_write_gamma(&memory,reinterpret_cast<std::uint64_t>(output),gamma);
 const int result=status==89?static_cast<int>(0x80290001U):status?static_cast<int>(0x80290002U):0;
 std::uint32_t bits=0;std::memcpy(&bits,&gamma,4);
 std::printf("{\"scope\":\"original_VideoOutSetGamma_return\",\"caller\":%llu,\"gamma_bits\":%u,\"result\":%d,\"renderer_adjustment\":false}\n",static_cast<unsigned long long>(caller),bits,result);std::fflush(stdout);return result;
}
extern "C" void ctr_sysv_video_gamma();
extern "C" int ctr_video_gamma_fixture(void*,float);
}
