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
inline int video_gamma_control(){
 GuestStack stack(32768,4096);auto* values=reinterpret_cast<float*>(stack.bottom());
 const std::array<float,4> initial{71,72,73,74};std::memcpy(values,initial.data(),sizeof(initial));
 if(ctr_video_gamma_fixture(values+1,1.25f) || (values[0]!=71 || values[1]!=1.25f || values[2]!=73 || values[3]!=74))return 1;
 if(ctr_video_gamma_fixture(values+1,2.1f)!=static_cast<int>(0x80290001U) || values[1]!=1.25f)return 2;
 if(ctr_video_gamma_fixture(nullptr,1.0f)!=static_cast<int>(0x80290002U))return 3;
 std::cout<<"{\"scope\":\"video_gamma_ABI\",\"passed\":true,\"only_first4_bytes_written\":true,\"renderer_adjustment\":false}\n";return 0;
}
}
