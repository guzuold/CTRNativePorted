#include "ctr/core/contact_curve_query.h"
#include <array>
#include <cmath>
#include <cstring>
#include <immintrin.h>
#include <stdexcept>
namespace {
__m128 loadv(const void*p,std::size_t n){__m128 v=_mm_setzero_ps();std::memcpy(&v,p,n);return v;}
void storev(void*p,__m128 v,std::size_t n){std::memcpy(p,&v,n);}
std::uint64_t loadi(const void*p,std::size_t n){std::uint64_t v=0;std::memcpy(&v,p,n);return v;}
void storei(void*p,std::uint64_t v,std::size_t n){std::memcpy(p,&v,n);}
}
namespace ctr {
float contact_curve_query(std::span<const float,32> control,std::span<const float,3> start,std::span<const float,3> end,std::span<const float,3> query,float first,float last,std::int32_t axis,float& projected,const std::uint64_t& guard){
 if(axis < -1 || axis>2)throw std::runtime_error("Missing out-of-range axis contract");
 alignas(16) std::array<std::byte,512> scratch{};
 bool cf=false,zf=false;
 std::uint64_t rax=0,rbx=0,rcx=0,rdx=0,rsi=0,rdi=0,r8=0,r9=0,r10=0,r11=0,rbp=0;
 rbp=reinterpret_cast<std::uint64_t>(scratch.data()+256);rdi=reinterpret_cast<std::uint64_t>(control.data());rsi=reinterpret_cast<std::uint64_t>(start.data());rdx=reinterpret_cast<std::uint64_t>(end.data());rcx=reinterpret_cast<std::uint64_t>(query.data());r8=reinterpret_cast<std::uint64_t>(&projected);r9=static_cast<std::uint32_t>(axis);
 __m128 x0=_mm_setzero_ps(),x1=_mm_setzero_ps(),x2=_mm_setzero_ps(),x3=_mm_setzero_ps(),x4=_mm_setzero_ps(),x5=_mm_setzero_ps(),x6=_mm_setzero_ps(),x7=_mm_setzero_ps(),x8=_mm_setzero_ps(),x9=_mm_setzero_ps(),x10=_mm_setzero_ps(),x11=_mm_setzero_ps(),x12=_mm_setzero_ps(),x13=_mm_setzero_ps(),x14=_mm_setzero_ps(),x15=_mm_setzero_ps();
 x0=_mm_set_ss(first);x1=_mm_set_ss(last);
 const std::array<unsigned char,4> c1b9e2ec={0,0,128,0};
 const std::array<unsigned char,4> c1b9e2f4={189,55,6,53};
 const std::array<unsigned char,4> c1b9e2f0={0,0,128,63};
 const std::array<unsigned char,16> c1baca70={255,255,255,127,255,255,255,127,255,255,255,127,255,255,255,127};
 const std::array<unsigned char,4> c1b9e2f8={0,0,64,64};
 const std::array<unsigned char,4> c1b9e300={0,0,0,63};
 const std::array<unsigned char,4> c1b9e2fc={189,55,134,53};
 const std::array<unsigned char,4> c1b9e304={0,0,0,0};
 // 0x13d3ddc mov rbx, qword ptr [rip + 0x120fa25]
 rbx=reinterpret_cast<std::uint64_t>(&guard);
 // 0x13d3de3 vxorps xmm8, xmm8, xmm8
 x8=_mm_xor_ps(x8,x8);
 // 0x13d3de8 mov rax, qword ptr [rbx]
 rax=loadi(reinterpret_cast<void*>(rbx),8);
 // 0x13d3deb mov qword ptr [rbp - 0x10], rax
 storei(reinterpret_cast<void*>(rbp - 0x10),rax,8);
 // 0x13d3def vmovss xmm2, dword ptr [rdx]
 x2=loadv(reinterpret_cast<void*>(rdx),4);
 // 0x13d3df3 vmovss xmm4, dword ptr [rsi]
 x4=loadv(reinterpret_cast<void*>(rsi),4);
 // 0x13d3df7 vmovsd xmm5, qword ptr [rsi + 4]
 x5=loadv(reinterpret_cast<void*>(rsi + 4),8);
 // 0x13d3dfc vsubss xmm6, xmm2, xmm4
 x6=_mm_sub_ss(x2,x4);
 // 0x13d3e00 vmovsd xmm2, qword ptr [rdx + 4]
 x2=loadv(reinterpret_cast<void*>(rdx + 4),8);
 // 0x13d3e05 vsubps xmm7, xmm2, xmm5
 x7=_mm_sub_ps(x2,x5);
 // 0x13d3e09 vmulss xmm2, xmm6, xmm6
 x2=_mm_mul_ss(x6,x6);
 // 0x13d3e0d vaddss xmm2, xmm2, xmm8
 x2=_mm_add_ss(x2,x8);
 // 0x13d3e12 vmulps xmm3, xmm7, xmm7
 x3=_mm_mul_ps(x7,x7);
 // 0x13d3e16 vaddss xmm2, xmm2, xmm3
 x2=_mm_add_ss(x2,x3);
 // 0x13d3e1a vmovshdup xmm3, xmm3
 x3=_mm_movehdup_ps(x3);
 // 0x13d3e1e vaddss xmm2, xmm2, xmm3
 x2=_mm_add_ss(x2,x3);
 // 0x13d3e22 vaddss xmm2, xmm2, dword ptr [rip + 0x7ca4c2]
 x2=_mm_add_ss(x2,loadv(c1b9e2ec.data(),4));
 // 0x13d3e2a vmovsd xmm3, qword ptr [rcx + 4]
 x3=loadv(reinterpret_cast<void*>(rcx + 4),8);
 // 0x13d3e2f vsqrtss xmm15, xmm2, xmm2
 x15=_mm_move_ss(x2,_mm_sqrt_ss(x2));
 // 0x13d3e33 vmovss xmm2, dword ptr [rcx]
 x2=loadv(reinterpret_cast<void*>(rcx),4);
 // 0x13d3e37 vsubss xmm2, xmm2, xmm4
 x2=_mm_sub_ss(x2,x4);
 // 0x13d3e3b vsubps xmm4, xmm3, xmm5
 x4=_mm_sub_ps(x3,x5);
 // 0x13d3e3f vmovss xmm5, dword ptr [rip + 0x7ca4ad]
 x5=loadv(c1b9e2f4.data(),4);
 // 0x13d3e47 vucomiss xmm5, xmm15
 cf=std::isnan(_mm_cvtss_f32(x5))||std::isnan(_mm_cvtss_f32(x15))||_mm_cvtss_f32(x5)<_mm_cvtss_f32(x15); zf=std::isnan(_mm_cvtss_f32(x5))||std::isnan(_mm_cvtss_f32(x15))||_mm_cvtss_f32(x5)==_mm_cvtss_f32(x15);
 // 0x13d3e4c jae 0x13d419f
 if(!cf) goto L13d419f;
 // 0x13d3e52 vmovss xmm14, dword ptr [rip + 0x7ca496]
 x14=loadv(c1b9e2f0.data(),4);
 // 0x13d3e5a vdivss xmm3, xmm14, xmm15
 x3=_mm_div_ss(x14,x15);
 // 0x13d3e5f vmulss xmm6, xmm3, xmm6
 x6=_mm_mul_ss(x3,x6);
 // 0x13d3e63 vmovsldup xmm3, xmm3
 x3=_mm_moveldup_ps(x3);
 // 0x13d3e67 vmulps xmm7, xmm3, xmm7
 x7=_mm_mul_ps(x3,x7);
 // 0x13d3e6b vmulss xmm2, xmm2, xmm6
 x2=_mm_mul_ss(x2,x6);
 // 0x13d3e6f vmulps xmm3, xmm4, xmm7
 x3=_mm_mul_ps(x4,x7);
 // 0x13d3e73 vaddss xmm2, xmm2, xmm8
 x2=_mm_add_ss(x2,x8);
 // 0x13d3e78 vaddss xmm2, xmm2, xmm3
 x2=_mm_add_ss(x2,x3);
 // 0x13d3e7c vmovshdup xmm3, xmm3
 x3=_mm_movehdup_ps(x3);
 // 0x13d3e80 vaddss xmm2, xmm2, xmm3
 x2=_mm_add_ss(x2,x3);
 // 0x13d3e84 vminss xmm3, xmm15, xmm2
 x3=_mm_min_ss(x15,x2);
 // 0x13d3e88 vcmpltss xmm2, xmm2, xmm8
 x2=_mm_cmplt_ss(x2,x8);
 // 0x13d3e8e vandnps xmm3, xmm2, xmm3
 x3=_mm_andnot_ps(x2,x3);
 // 0x13d3e92 vandps xmm2, xmm3, xmmword ptr [rip + 0x7d8bd6]
 x2=_mm_and_ps(x3,loadv(c1baca70.data(),16));
 // 0x13d3e9a vmovss dword ptr [r8], xmm3
 storev(reinterpret_cast<void*>(r8),x3,4);
 // 0x13d3e9f vucomiss xmm5, xmm2
 cf=std::isnan(_mm_cvtss_f32(x5))||std::isnan(_mm_cvtss_f32(x2))||_mm_cvtss_f32(x5)<_mm_cvtss_f32(x2); zf=std::isnan(_mm_cvtss_f32(x5))||std::isnan(_mm_cvtss_f32(x2))||_mm_cvtss_f32(x5)==_mm_cvtss_f32(x2);
 // 0x13d3ea3 jae 0x13d3eb8
 if(!cf) goto L13d3eb8;
 // 0x13d3ea5 vsubss xmm2, xmm3, xmm15
 x2=_mm_sub_ss(x3,x15);
 // 0x13d3eaa vandps xmm2, xmm2, xmmword ptr [rip + 0x7d8bbe]
 x2=_mm_and_ps(x2,loadv(c1baca70.data(),16));
 // 0x13d3eb2 vucomiss xmm5, xmm2
 cf=std::isnan(_mm_cvtss_f32(x5))||std::isnan(_mm_cvtss_f32(x2))||_mm_cvtss_f32(x5)<_mm_cvtss_f32(x2); zf=std::isnan(_mm_cvtss_f32(x5))||std::isnan(_mm_cvtss_f32(x2))||_mm_cvtss_f32(x5)==_mm_cvtss_f32(x2);
 // 0x13d3eb6 jb 0x13d3f23
 if(cf) goto L13d3f23;
L13d3eb8:;
 // 0x13d3eb8 vmulss xmm0, xmm3, xmm6
 x0=_mm_mul_ss(x3,x6);
 // 0x13d3ebc vaddss xmm0, xmm0, dword ptr [rsi]
 x0=_mm_add_ss(x0,loadv(reinterpret_cast<void*>(rsi),4));
 // 0x13d3ec0 vmovshdup xmm2, xmm7
 x2=_mm_movehdup_ps(x7);
 // 0x13d3ec4 vmulss xmm1, xmm3, xmm7
 x1=_mm_mul_ss(x3,x7);
 // 0x13d3ec8 vaddss xmm1, xmm1, dword ptr [rsi + 4]
 x1=_mm_add_ss(x1,loadv(reinterpret_cast<void*>(rsi + 4),4));
 // 0x13d3ecd vmulss xmm2, xmm3, xmm2
 x2=_mm_mul_ss(x3,x2);
 // 0x13d3ed1 vmovss xmm3, dword ptr [rcx]
 x3=loadv(reinterpret_cast<void*>(rcx),4);
 // 0x13d3ed5 vaddss xmm2, xmm2, dword ptr [rsi + 8]
 x2=_mm_add_ss(x2,loadv(reinterpret_cast<void*>(rsi + 8),4));
 // 0x13d3eda vsubss xmm0, xmm3, xmm0
 x0=_mm_sub_ss(x3,x0);
 // 0x13d3ede vmovss xmm3, dword ptr [rcx + 4]
 x3=loadv(reinterpret_cast<void*>(rcx + 4),4);
 // 0x13d3ee3 vmulss xmm0, xmm0, xmm0
 x0=_mm_mul_ss(x0,x0);
 // 0x13d3ee7 vsubss xmm1, xmm3, xmm1
 x1=_mm_sub_ss(x3,x1);
 // 0x13d3eeb vxorps xmm3, xmm3, xmm3
 x3=_mm_xor_ps(x3,x3);
 // 0x13d3eef vaddss xmm0, xmm0, xmm3
 x0=_mm_add_ss(x0,x3);
 // 0x13d3ef3 vmulss xmm1, xmm1, xmm1
 x1=_mm_mul_ss(x1,x1);
 // 0x13d3ef7 vaddss xmm0, xmm0, xmm1
 x0=_mm_add_ss(x0,x1);
 // 0x13d3efb vmovss xmm1, dword ptr [rcx + 8]
 x1=loadv(reinterpret_cast<void*>(rcx + 8),4);
 // 0x13d3f00 vsubss xmm1, xmm1, xmm2
 x1=_mm_sub_ss(x1,x2);
L13d3f04:;
 // 0x13d3f04 vmulss xmm1, xmm1, xmm1
 x1=_mm_mul_ss(x1,x1);
 // 0x13d3f08 vaddss xmm0, xmm0, xmm1
 x0=_mm_add_ss(x0,x1);
L13d3f0c:;
 // 0x13d3f0c mov rax, qword ptr [rbx]
 rax=loadi(reinterpret_cast<void*>(rbx),8);
 // 0x13d3f0f cmp rax, qword ptr [rbp - 0x10]
 zf=rax==loadi(reinterpret_cast<void*>(rbp - 0x10),8);
 // 0x13d3f13 jne 0x13d4199
 if(!zf) goto L13d4199;
 // 0x13d3f22 ret
 return _mm_cvtss_f32(x0);
L13d3f23:;
 // 0x13d3f23 vdivss xmm2, xmm3, xmm15
 x2=_mm_div_ss(x3,x15);
 // 0x13d3f28 vsubss xmm3, xmm1, xmm0
 x3=_mm_sub_ss(x1,x0);
 // 0x13d3f2c vmovss xmm4, dword ptr [rcx + 4]
 x4=loadv(reinterpret_cast<void*>(rcx + 4),4);
 // 0x13d3f31 vxorps xmm5, xmm5, xmm5
 x5=_mm_xor_ps(x5,x5);
 // 0x13d3f35 cmp r9d, -1
 zf=r9==static_cast<std::uint32_t>(-1);
 // 0x13d3f39 vmovss dword ptr [rbp - 0x3c], xmm3
 storev(reinterpret_cast<void*>(rbp - 0x3c),x3,4);
 // 0x13d3f3e vmovss dword ptr [rbp - 0x44], xmm4
 storev(reinterpret_cast<void*>(rbp - 0x44),x4,4);
 // 0x13d3f43 vsubss xmm4, xmm4, dword ptr [rsi + 4]
 x4=_mm_sub_ss(x4,loadv(reinterpret_cast<void*>(rsi + 4),4));
 // 0x13d3f48 vmulss xmm4, xmm4, xmm4
 x4=_mm_mul_ss(x4,x4);
 // 0x13d3f4c vmulss xmm2, xmm3, xmm2
 x2=_mm_mul_ss(x3,x2);
 // 0x13d3f50 vmovss xmm3, dword ptr [rcx]
 x3=loadv(reinterpret_cast<void*>(rcx),4);
 // 0x13d3f54 vmovss dword ptr [rbp - 0x48], xmm3
 storev(reinterpret_cast<void*>(rbp - 0x48),x3,4);
 // 0x13d3f59 vsubss xmm3, xmm3, dword ptr [rsi]
 x3=_mm_sub_ss(x3,loadv(reinterpret_cast<void*>(rsi),4));
 // 0x13d3f5d vmulss xmm3, xmm3, xmm3
 x3=_mm_mul_ss(x3,x3);
 // 0x13d3f61 vaddss xmm3, xmm3, xmm5
 x3=_mm_add_ss(x3,x5);
 // 0x13d3f65 vaddss xmm3, xmm3, xmm4
 x3=_mm_add_ss(x3,x4);
 // 0x13d3f69 vmovss xmm4, dword ptr [rcx + 8]
 x4=loadv(reinterpret_cast<void*>(rcx + 8),4);
 // 0x13d3f6e vmovss dword ptr [rbp - 0x40], xmm4
 storev(reinterpret_cast<void*>(rbp - 0x40),x4,4);
 // 0x13d3f73 vsubss xmm4, xmm4, dword ptr [rsi + 8]
 x4=_mm_sub_ss(x4,loadv(reinterpret_cast<void*>(rsi + 8),4));
 // 0x13d3f78 vmulss xmm4, xmm4, xmm4
 x4=_mm_mul_ss(x4,x4);
 // 0x13d3f7c vaddss xmm10, xmm3, xmm4
 x10=_mm_add_ss(x3,x4);
 // 0x13d3f80 vaddss xmm3, xmm2, xmm0
 x3=_mm_add_ss(x2,x0);
 // 0x13d3f84 vmulss xmm5, xmm3, dword ptr [rip + 0x7ca36c]
 x5=_mm_mul_ss(x3,loadv(c1b9e2f8.data(),4));
 // 0x13d3f8c vsubss xmm2, xmm14, xmm3
 x2=_mm_sub_ss(x14,x3);
 // 0x13d3f90 vmovss dword ptr [rbp - 0x1c], xmm10
 storev(reinterpret_cast<void*>(rbp - 0x1c),x10,4);
 // 0x13d3f95 vmovss xmm7, dword ptr [rdi + 0x50]
 x7=loadv(reinterpret_cast<void*>(rdi + 0x50),4);
 // 0x13d3f9a vmulss xmm4, xmm2, xmm2
 x4=_mm_mul_ss(x2,x2);
 // 0x13d3f9e vmulss xmm6, xmm5, xmm2
 x6=_mm_mul_ss(x5,x2);
 // 0x13d3fa2 vmulss xmm8, xmm3, xmm5
 x8=_mm_mul_ss(x3,x5);
 // 0x13d3fa6 vmovss xmm5, dword ptr [rdi + 0x54]
 x5=loadv(reinterpret_cast<void*>(rdi + 0x54),4);
 // 0x13d3fab vmulss xmm4, xmm2, xmm4
 x4=_mm_mul_ss(x2,x4);
 // 0x13d3faf vmulss xmm6, xmm2, xmm6
 x6=_mm_mul_ss(x2,x6);
 // 0x13d3fb3 vmovss dword ptr [rbp - 0x80], xmm7
 storev(reinterpret_cast<void*>(rbp - 0x80),x7,4);
 // 0x13d3fb8 vmulss xmm2, xmm2, xmm8
 x2=_mm_mul_ss(x2,x8);
 // 0x13d3fbd vmulss xmm7, xmm4, xmm7
 x7=_mm_mul_ss(x4,x7);
 // 0x13d3fc1 vmovss dword ptr [rbp - 0x7c], xmm5
 storev(reinterpret_cast<void*>(rbp - 0x7c),x5,4);
 // 0x13d3fc6 vmulss xmm9, xmm4, xmm5
 x9=_mm_mul_ss(x4,x5);
 // 0x13d3fca vmovss xmm5, dword ptr [rdi + 0x5c]
 x5=loadv(reinterpret_cast<void*>(rdi + 0x5c),4);
 // 0x13d3fcf vmovss dword ptr [rbp - 0x78], xmm5
 storev(reinterpret_cast<void*>(rbp - 0x78),x5,4);
 // 0x13d3fd4 vmulss xmm5, xmm6, xmm5
 x5=_mm_mul_ss(x6,x5);
 // 0x13d3fd8 vaddss xmm5, xmm7, xmm5
 x5=_mm_add_ss(x7,x5);
 // 0x13d3fdc vmovss xmm7, dword ptr [rdi + 0x60]
 x7=loadv(reinterpret_cast<void*>(rdi + 0x60),4);
 // 0x13d3fe1 vmovss dword ptr [rbp - 0x74], xmm7
 storev(reinterpret_cast<void*>(rbp - 0x74),x7,4);
 // 0x13d3fe6 vmulss xmm7, xmm6, xmm7
 x7=_mm_mul_ss(x6,x7);
 // 0x13d3fea vaddss xmm8, xmm9, xmm7
 x8=_mm_add_ss(x9,x7);
 // 0x13d3fee vmovss xmm7, dword ptr [rdi + 0x58]
 x7=loadv(reinterpret_cast<void*>(rdi + 0x58),4);
 // 0x13d3ff3 vmovaps xmm9, xmm3
 x9=x3;
 // 0x13d3ff7 vmovss dword ptr [rbp - 0x70], xmm7
 storev(reinterpret_cast<void*>(rbp - 0x70),x7,4);
 // 0x13d3ffc vmulss xmm4, xmm4, xmm7
 x4=_mm_mul_ss(x4,x7);
 // 0x13d4000 vmovss xmm7, dword ptr [rdi + 0x64]
 x7=loadv(reinterpret_cast<void*>(rdi + 0x64),4);
 // 0x13d4005 vmulss xmm6, xmm6, xmm7
 x6=_mm_mul_ss(x6,x7);
 // 0x13d4009 vmovss dword ptr [rbp - 0x6c], xmm7
 storev(reinterpret_cast<void*>(rbp - 0x6c),x7,4);
 // 0x13d400e vaddss xmm4, xmm4, xmm6
 x4=_mm_add_ss(x4,x6);
 // 0x13d4012 vmovss xmm6, dword ptr [rdi + 0x68]
 x6=loadv(reinterpret_cast<void*>(rdi + 0x68),4);
 // 0x13d4017 vmovss dword ptr [rbp - 0x68], xmm6
 storev(reinterpret_cast<void*>(rbp - 0x68),x6,4);
 // 0x13d401c vmulss xmm6, xmm2, xmm6
 x6=_mm_mul_ss(x2,x6);
 // 0x13d4020 vaddss xmm5, xmm5, xmm6
 x5=_mm_add_ss(x5,x6);
 // 0x13d4024 vmovss xmm6, dword ptr [rdi + 0x6c]
 x6=loadv(reinterpret_cast<void*>(rdi + 0x6c),4);
 // 0x13d4029 vmovss dword ptr [rbp - 0x64], xmm6
 storev(reinterpret_cast<void*>(rbp - 0x64),x6,4);
 // 0x13d402e vmulss xmm6, xmm2, xmm6
 x6=_mm_mul_ss(x2,x6);
 // 0x13d4032 vaddss xmm7, xmm8, xmm6
 x7=_mm_add_ss(x8,x6);
 // 0x13d4036 vmovss xmm6, dword ptr [rdi + 0x70]
 x6=loadv(reinterpret_cast<void*>(rdi + 0x70),4);
 // 0x13d403b vmulss xmm2, xmm2, xmm6
 x2=_mm_mul_ss(x2,x6);
 // 0x13d403f vmovss dword ptr [rbp - 0x60], xmm6
 storev(reinterpret_cast<void*>(rbp - 0x60),x6,4);
 // 0x13d4044 vmovss xmm6, dword ptr [rdi + 0x74]
 x6=loadv(reinterpret_cast<void*>(rdi + 0x74),4);
 // 0x13d4049 vaddss xmm2, xmm4, xmm2
 x2=_mm_add_ss(x4,x2);
 // 0x13d404d vmulss xmm4, xmm3, xmm3
 x4=_mm_mul_ss(x3,x3);
 // 0x13d4051 vmulss xmm4, xmm3, xmm4
 x4=_mm_mul_ss(x3,x4);
 // 0x13d4055 vmovss xmm3, dword ptr [rdi + 0x7c]
 x3=loadv(reinterpret_cast<void*>(rdi + 0x7c),4);
 // 0x13d405a vmovss dword ptr [rbp - 0x5c], xmm6
 storev(reinterpret_cast<void*>(rbp - 0x5c),x6,4);
 // 0x13d405f vmulss xmm6, xmm4, xmm6
 x6=_mm_mul_ss(x4,x6);
 // 0x13d4063 vaddss xmm5, xmm5, xmm6
 x5=_mm_add_ss(x5,x6);
 // 0x13d4067 vmovss xmm6, dword ptr [rdi + 0x78]
 x6=loadv(reinterpret_cast<void*>(rdi + 0x78),4);
 // 0x13d406c vmovss dword ptr [rbp - 0x54], xmm3
 storev(reinterpret_cast<void*>(rbp - 0x54),x3,4);
 // 0x13d4071 vmulss xmm3, xmm4, xmm3
 x3=_mm_mul_ss(x4,x3);
 // 0x13d4075 vmovss dword ptr [rbp - 0x28], xmm5
 storev(reinterpret_cast<void*>(rbp - 0x28),x5,4);
 // 0x13d407a vmovss dword ptr [rbp - 0x58], xmm6
 storev(reinterpret_cast<void*>(rbp - 0x58),x6,4);
 // 0x13d407f vmulss xmm6, xmm4, xmm6
 x6=_mm_mul_ss(x4,x6);
 // 0x13d4083 vaddss xmm6, xmm7, xmm6
 x6=_mm_add_ss(x7,x6);
 // 0x13d4087 vaddss xmm7, xmm2, xmm3
 x7=_mm_add_ss(x2,x3);
 // 0x13d408b vmovss dword ptr [rbp - 0x24], xmm6
 storev(reinterpret_cast<void*>(rbp - 0x24),x6,4);
 // 0x13d4090 vmovss dword ptr [rbp - 0x20], xmm7
 storev(reinterpret_cast<void*>(rbp - 0x20),x7,4);
 // 0x13d4095 je 0x13d41b9
 if(zf) goto L13d41b9;
 // 0x13d409b movsxd rcx, r9d
 rcx=static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(r9)));
 // 0x13d409e mov dword ptr [rbp + rcx*4 - 0x28], 0
 storei(reinterpret_cast<void*>(rbp + rcx*4 - 0x28),0,4);
 // 0x13d40a6 vmovss xmm5, dword ptr [rbp - 0x28]
 x5=loadv(reinterpret_cast<void*>(rbp - 0x28),4);
 // 0x13d40ab vmovss xmm6, dword ptr [rbp - 0x24]
 x6=loadv(reinterpret_cast<void*>(rbp - 0x24),4);
 // 0x13d40b0 vmovss xmm7, dword ptr [rbp - 0x20]
 x7=loadv(reinterpret_cast<void*>(rbp - 0x20),4);
L13d40b5:;
 // 0x13d40b5 vmovss xmm4, dword ptr [rbp - 0x48]
 x4=loadv(reinterpret_cast<void*>(rbp - 0x48),4);
 // 0x13d40ba vmovss xmm11, dword ptr [rbp - 0x44]
 x11=loadv(reinterpret_cast<void*>(rbp - 0x44),4);
 // 0x13d40bf vxorps xmm8, xmm8, xmm8
 x8=_mm_xor_ps(x8,x8);
 // 0x13d40c4 vmovss xmm13, dword ptr [rip + 0x7ca234]
 x13=loadv(c1b9e300.data(),4);
 // 0x13d40cc movabs r11, 0x7ff0000000000000
 r11=0x7ff0000000000000;
 // 0x13d40d6 movabs r10, 0xfffff00000000
 r10=0xfffff00000000;
 // 0x13d40e0 mov edi, 2
 rdi=static_cast<std::uint32_t>(2);
 // 0x13d40e5 vsubss xmm2, xmm4, xmm5
 x2=_mm_sub_ss(x4,x5);
 // 0x13d40e9 vsubss xmm3, xmm11, xmm6
 x3=_mm_sub_ss(x11,x6);
 // 0x13d40ed vmovss xmm6, dword ptr [rbp - 0x40]
 x6=loadv(reinterpret_cast<void*>(rbp - 0x40),4);
 // 0x13d40f2 vmulss xmm2, xmm2, xmm2
 x2=_mm_mul_ss(x2,x2);
 // 0x13d40f6 vmulss xmm3, xmm3, xmm3
 x3=_mm_mul_ss(x3,x3);
 // 0x13d40fa vaddss xmm2, xmm2, xmm8
 x2=_mm_add_ss(x2,x8);
 // 0x13d40ff vaddss xmm2, xmm2, xmm3
 x2=_mm_add_ss(x2,x3);
 // 0x13d4103 vsubss xmm3, xmm6, xmm7
 x3=_mm_sub_ss(x6,x7);
 // 0x13d4107 vmulss xmm3, xmm3, xmm3
 x3=_mm_mul_ss(x3,x3);
 // 0x13d410b vaddss xmm7, xmm2, xmm3
 x7=_mm_add_ss(x2,x3);
 // 0x13d410f vmovss dword ptr [rbp - 0x18], xmm7
 storev(reinterpret_cast<void*>(rbp - 0x18),x7,4);
 // 0x13d4114 vsubss xmm2, xmm4, dword ptr [rdx]
 x2=_mm_sub_ss(x4,loadv(reinterpret_cast<void*>(rdx),4));
 // 0x13d4118 vsubss xmm3, xmm11, dword ptr [rdx + 4]
 x3=_mm_sub_ss(x11,loadv(reinterpret_cast<void*>(rdx + 4),4));
 // 0x13d411d vmovaps xmm4, xmm9
 x4=x9;
 // 0x13d4121 vmovss dword ptr [rbp - 0x4c], xmm4
 storev(reinterpret_cast<void*>(rbp - 0x4c),x4,4);
 // 0x13d4126 vmulss xmm2, xmm2, xmm2
 x2=_mm_mul_ss(x2,x2);
 // 0x13d412a vmulss xmm3, xmm3, xmm3
 x3=_mm_mul_ss(x3,x3);
 // 0x13d412e vaddss xmm2, xmm2, xmm8
 x2=_mm_add_ss(x2,x8);
 // 0x13d4133 vaddss xmm2, xmm2, xmm3
 x2=_mm_add_ss(x2,x3);
 // 0x13d4137 vsubss xmm3, xmm6, dword ptr [rdx + 8]
 x3=_mm_sub_ss(x6,loadv(reinterpret_cast<void*>(rdx + 8),4));
 // 0x13d413c mov edx, 0xfffffffd
 rdx=static_cast<std::uint32_t>(0xfffffffd);
 // 0x13d4141 vmovss dword ptr [rbp - 0x28], xmm0
 storev(reinterpret_cast<void*>(rbp - 0x28),x0,4);
 // 0x13d4146 vmovss dword ptr [rbp - 0x24], xmm4
 storev(reinterpret_cast<void*>(rbp - 0x24),x4,4);
 // 0x13d414b vmovss dword ptr [rbp - 0x20], xmm1
 storev(reinterpret_cast<void*>(rbp - 0x20),x1,4);
 // 0x13d4150 vmulss xmm3, xmm3, xmm3
 x3=_mm_mul_ss(x3,x3);
 // 0x13d4154 vaddss xmm3, xmm2, xmm3
 x3=_mm_add_ss(x2,x3);
 // 0x13d4158 vmovss xmm2, dword ptr [rbp - 0x3c]
 x2=loadv(reinterpret_cast<void*>(rbp - 0x3c),4);
 // 0x13d415d vmulss xmm2, xmm2, dword ptr [rip + 0x7ca197]
 x2=_mm_mul_ss(x2,loadv(c1b9e2fc.data(),4));
 // 0x13d4165 vinsertps xmm6, xmm3, xmm10, 0x10
 x6=_mm_insert_ps(x3,x10,0x10);
 // 0x13d416b vmovss dword ptr [rbp - 0x14], xmm3
 storev(reinterpret_cast<void*>(rbp - 0x14),x3,4);
 // 0x13d4170 vmovaps xmm10, xmm7
 x10=x7;
 // 0x13d4174 vinsertps xmm3, xmm0, xmm1, 0x10
 x3=_mm_insert_ps(x0,x1,0x10);
 // 0x13d417a vaddss xmm11, xmm2, xmm0
 x11=_mm_add_ss(x2,x0);
 // 0x13d417e vsubss xmm9, xmm1, xmm2
 x9=_mm_sub_ss(x1,x2);
 // 0x13d4182 vmovss dword ptr [rbp - 0x84], xmm2
 storev(reinterpret_cast<void*>(rbp - 0x84),x2,4);
 // 0x13d418a vmovaps xmmword ptr [rbp - 0xd0], xmm11
 storev(reinterpret_cast<void*>(rbp - 0xd0),x11,16);
 // 0x13d4192 vmovss dword ptr [rbp - 0x50], xmm9
 storev(reinterpret_cast<void*>(rbp - 0x50),x9,4);
 // 0x13d4197 jmp 0x13d4203
 goto L13d4203;
L13d4199:;
 // 0x13d4199 call 0x1adb0d0
 throw std::runtime_error("Missing original stack guard failure service1adb0d0");
L13d419f:;
 // 0x13d419f vmulss xmm0, xmm2, xmm2
 x0=_mm_mul_ss(x2,x2);
 // 0x13d41a3 vmulss xmm1, xmm4, xmm4
 x1=_mm_mul_ss(x4,x4);
 // 0x13d41a7 vaddss xmm0, xmm0, xmm8
 x0=_mm_add_ss(x0,x8);
 // 0x13d41ac vaddss xmm0, xmm0, xmm1
 x0=_mm_add_ss(x0,x1);
 // 0x13d41b0 vmovshdup xmm1, xmm4
 x1=_mm_movehdup_ps(x4);
 // 0x13d41b4 jmp 0x13d3f04
 goto L13d3f04;
L13d41b9:;
 // 0x13d41b9 mov rcx, 0xffffffffffffffff
 rcx=0xffffffffffffffff;
 // 0x13d41c0 jmp 0x13d40b5
 goto L13d40b5;
L13d41c5:;
 // 0x13d41c5 vmovss xmm3, dword ptr [rbp - 0x14]
 x3=loadv(reinterpret_cast<void*>(rbp - 0x14),4);
 // 0x13d41ca vmovss xmm2, dword ptr [rbp - 0x28]
 x2=loadv(reinterpret_cast<void*>(rbp - 0x28),4);
 // 0x13d41cf vmovss xmm4, dword ptr [rbp - 0x24]
 x4=loadv(reinterpret_cast<void*>(rbp - 0x24),4);
 // 0x13d41d4 vmovss xmm10, dword ptr [rbp - 0x18]
 x10=loadv(reinterpret_cast<void*>(rbp - 0x18),4);
 // 0x13d41d9 vmovaps xmm11, xmmword ptr [rbp - 0xd0]
 x11=loadv(reinterpret_cast<void*>(rbp - 0xd0),16);
 // 0x13d41e1 vmovss xmm9, dword ptr [rbp - 0x50]
 x9=loadv(reinterpret_cast<void*>(rbp - 0x50),4);
 // 0x13d41e6 vmovss xmm13, dword ptr [rip + 0x7ca112]
 x13=loadv(c1b9e300.data(),4);
 // 0x13d41ee inc edx
 rdx=static_cast<std::uint32_t>(rdx+1);zf=rdx==0;
 // 0x13d41f0 vmovss dword ptr [rbp - 0x4c], xmm1
 storev(reinterpret_cast<void*>(rbp - 0x4c),x1,4);
 // 0x13d41f5 vinsertps xmm6, xmm3, dword ptr [rbp - 0x1c], 0x10
 x6=_mm_insert_ps(x3,loadv(reinterpret_cast<void*>(rbp - 0x1c),4),0x10);
 // 0x13d41fc vinsertps xmm3, xmm2, dword ptr [rbp - 0x20], 0x10
 x3=_mm_insert_ps(x2,loadv(reinterpret_cast<void*>(rbp - 0x20),4),0x10);
L13d4203:;
 // 0x13d4203 vmovshdup xmm12, xmm3
 x12=_mm_movehdup_ps(x3);
 // 0x13d4207 vmulss xmm1, xmm4, xmm4
 x1=_mm_mul_ss(x4,x4);
 // 0x13d420b vmulss xmm7, xmm3, xmm3
 x7=_mm_mul_ss(x3,x3);
 // 0x13d420f vmovshdup xmm2, xmm6
 x2=_mm_movehdup_ps(x6);
 // 0x13d4213 vmovaps xmmword ptr [rbp - 0xe0], xmm4
 storev(reinterpret_cast<void*>(rbp - 0xe0),x4,16);
 // 0x13d421b vmovaps xmmword ptr [rbp - 0xc0], xmm10
 storev(reinterpret_cast<void*>(rbp - 0xc0),x10,16);
 // 0x13d4223 vmovaps xmmword ptr [rbp - 0xb0], xmm6
 storev(reinterpret_cast<void*>(rbp - 0xb0),x6,16);
 // 0x13d422b vmulss xmm5, xmm12, xmm12
 x5=_mm_mul_ss(x12,x12);
 // 0x13d4230 vmovaps xmmword ptr [rbp - 0xa0], xmm2
 storev(reinterpret_cast<void*>(rbp - 0xa0),x2,16);
 // 0x13d4238 vinsertps xmm1, xmm1, xmm4, 0x10
 x1=_mm_insert_ps(x1,x4,0x10);
 // 0x13d423e vinsertps xmm7, xmm7, xmm3, 0x1c
 x7=_mm_insert_ps(x7,x3,0x1c);
 // 0x13d4244 vblendps xmm5, xmm3, xmm5, 1
 x5=_mm_blend_ps(x3,x5,1);
 // 0x13d424a vsubps xmm8, xmm1, xmm5
 x8=_mm_sub_ps(x1,x5);
 // 0x13d424e vsubps xmm5, xmm5, xmm7
 x5=_mm_sub_ps(x5,x7);
 // 0x13d4252 vmulps xmm4, xmm2, xmm8
 x4=_mm_mul_ps(x2,x8);
 // 0x13d4257 vmovsldup xmm2, xmm10
 x2=_mm_moveldup_ps(x10);
 // 0x13d425c vsubps xmm10, xmm7, xmm1
 x10=_mm_sub_ps(x7,x1);
 // 0x13d4260 vmovsldup xmm1, xmm6
 x1=_mm_moveldup_ps(x6);
 // 0x13d4264 vmulps xmm2, xmm2, xmm5
 x2=_mm_mul_ps(x2,x5);
 // 0x13d4268 vaddps xmm2, xmm4, xmm2
 x2=_mm_add_ps(x4,x2);
 // 0x13d426c vmulps xmm1, xmm10, xmm1
 x1=_mm_mul_ps(x10,x1);
 // 0x13d4270 vaddps xmm1, xmm1, xmm2
 x1=_mm_add_ps(x1,x2);
 // 0x13d4274 vmulss xmm2, xmm1, xmm13
 x2=_mm_mul_ss(x1,x13);
 // 0x13d4279 vmovshdup xmm1, xmm1
 x1=_mm_movehdup_ps(x1);
 // 0x13d427d vdivss xmm1, xmm2, xmm1
 x1=_mm_div_ss(x2,x1);
 // 0x13d4281 vminss xmm2, xmm9, xmm1
 x2=_mm_min_ss(x9,x1);
 // 0x13d4285 vcmpltss xmm1, xmm1, xmm11
 x1=_mm_cmplt_ss(x1,x11);
 // 0x13d428b vblendvps xmm1, xmm2, xmm11, xmm1
 x1=_mm_blendv_ps(x2,x11,x1);
 // 0x13d4291 vcvtss2sd xmm2, xmm1, xmm1
 x2=_mm_castpd_ps(_mm_cvtss_sd(_mm_castps_pd(x1),x1));
 // 0x13d4295 vmovq rsi, xmm2
 rsi=static_cast<std::uint64_t>(_mm_cvtsi128_si64(_mm_castps_si128(x2)));
 // 0x13d429a andn rax, rsi, r11
 rax=(~rsi)&r11;zf=rax==0;cf=false;
 // 0x13d429f je 0x13d4483
 if(zf) goto L13d4483;
L13d42a5:;
 // 0x13d42a5 vsubss xmm2, xmm1, dword ptr [rbp - 0x4c]
 x2=_mm_sub_ss(x1,loadv(reinterpret_cast<void*>(rbp - 0x4c),4));
 // 0x13d42aa vmovss xmm4, dword ptr [rbp - 0x84]
 x4=loadv(reinterpret_cast<void*>(rbp - 0x84),4);
 // 0x13d42b2 vandps xmm2, xmm2, xmmword ptr [rip + 0x7d87b6]
 x2=_mm_and_ps(x2,loadv(c1baca70.data(),16));
 // 0x13d42ba vucomiss xmm4, xmm2
 cf=std::isnan(_mm_cvtss_f32(x4))||std::isnan(_mm_cvtss_f32(x2))||_mm_cvtss_f32(x4)<_mm_cvtss_f32(x2); zf=std::isnan(_mm_cvtss_f32(x4))||std::isnan(_mm_cvtss_f32(x2))||_mm_cvtss_f32(x4)==_mm_cvtss_f32(x2);
 // 0x13d42be jae 0x13d4490
 if(!cf) goto L13d4490;
 // 0x13d42c4 vmovaps xmm4, xmmword ptr [rbp - 0xe0]
 x4=loadv(reinterpret_cast<void*>(rbp - 0xe0),16);
 // 0x13d42cc vmovaps xmm11, xmm5
 x11=x5;
 // 0x13d42d0 vmovsldup xmm5, xmm1
 x5=_mm_moveldup_ps(x1);
 // 0x13d42d4 vsubss xmm9, xmm3, xmm12
 x9=_mm_sub_ss(x3,x12);
 // 0x13d42d9 vmovshdup xmm6, xmm8
 x6=_mm_movehdup_ps(x8);
 // 0x13d42de xor esi, esi
 rsi=0;zf=true;cf=false;
 // 0x13d42e0 vsubps xmm5, xmm5, xmm3
 x5=_mm_sub_ps(x5,x3);
 // 0x13d42e4 vmovshdup xmm7, xmm5
 x7=_mm_movehdup_ps(x5);
 // 0x13d42e8 vsubss xmm2, xmm1, xmm4
 x2=_mm_sub_ss(x1,x4);
 // 0x13d42ec vmulss xmm7, xmm5, xmm7
 x7=_mm_mul_ss(x5,x7);
 // 0x13d42f0 vsubss xmm3, xmm4, xmm3
 x3=_mm_sub_ss(x4,x3);
 // 0x13d42f4 vmovsldup xmm2, xmm2
 x2=_mm_moveldup_ps(x2);
 // 0x13d42f8 vmulss xmm3, xmm6, xmm3
 x3=_mm_mul_ss(x6,x3);
 // 0x13d42fc vmovaps xmm6, xmmword ptr [rbp - 0xc0]
 x6=loadv(reinterpret_cast<void*>(rbp - 0xc0),16);
 // 0x13d4304 vmulps xmm2, xmm5, xmm2
 x2=_mm_mul_ps(x5,x2);
 // 0x13d4308 vsubss xmm5, xmm12, xmm4
 x5=_mm_sub_ss(x12,x4);
 // 0x13d430c vmovshdup xmm4, xmm11
 x4=_mm_movehdup_ps(x11);
 // 0x13d4311 vinsertps xmm4, xmm4, xmm9, 0x10
 x4=_mm_insert_ps(x4,x9,0x10);
 // 0x13d4317 vdivss xmm3, xmm7, xmm3
 x3=_mm_div_ss(x7,x3);
 // 0x13d431b vblendps xmm5, xmm10, xmm5, 1
 x5=_mm_blend_ps(x10,x5,1);
 // 0x13d4321 vmulps xmm4, xmm5, xmm4
 x4=_mm_mul_ps(x5,x4);
 // 0x13d4325 vmovaps xmm5, xmmword ptr [rbp - 0xb0]
 x5=loadv(reinterpret_cast<void*>(rbp - 0xb0),16);
 // 0x13d432d vmovaps xmm7, xmm6
 x7=x6;
 // 0x13d4331 vdivps xmm2, xmm2, xmm4
 x2=_mm_div_ps(x2,x4);
 // 0x13d4335 vmulss xmm3, xmm6, xmm3
 x3=_mm_mul_ss(x6,x3);
 // 0x13d4339 vmulps xmm2, xmm5, xmm2
 x2=_mm_mul_ps(x5,x2);
 // 0x13d433d vmovshdup xmm4, xmm2
 x4=_mm_movehdup_ps(x2);
 // 0x13d4341 vaddss xmm3, xmm4, xmm3
 x3=_mm_add_ss(x4,x3);
 // 0x13d4345 vaddss xmm2, xmm2, xmm3
 x2=_mm_add_ss(x2,x3);
 // 0x13d4349 vmovaps xmm3, xmmword ptr [rbp - 0xa0]
 x3=loadv(reinterpret_cast<void*>(rbp - 0xa0),16);
 // 0x13d4351 vucomiss xmm7, xmm3
 cf=std::isnan(_mm_cvtss_f32(x7))||std::isnan(_mm_cvtss_f32(x3))||_mm_cvtss_f32(x7)<_mm_cvtss_f32(x3); zf=std::isnan(_mm_cvtss_f32(x7))||std::isnan(_mm_cvtss_f32(x3))||_mm_cvtss_f32(x7)==_mm_cvtss_f32(x3);
 // 0x13d4355 seta sil
 rsi=(!cf&&!zf)?1:0;
 // 0x13d4359 vucomiss xmm5, dword ptr [rbp + rsi*4 - 0x1c]
 cf=std::isnan(_mm_cvtss_f32(x5))||std::isnan(_mm_cvtss_f32(loadv(reinterpret_cast<void*>(rbp + rsi*4 - 0x1c),4)))||_mm_cvtss_f32(x5)<_mm_cvtss_f32(loadv(reinterpret_cast<void*>(rbp + rsi*4 - 0x1c),4)); zf=std::isnan(_mm_cvtss_f32(x5))||std::isnan(_mm_cvtss_f32(loadv(reinterpret_cast<void*>(rbp + rsi*4 - 0x1c),4)))||_mm_cvtss_f32(x5)==_mm_cvtss_f32(loadv(reinterpret_cast<void*>(rbp + rsi*4 - 0x1c),4));
 // 0x13d435f cmova rsi, rdi
 if(!cf&&!zf) rsi=rdi;
 // 0x13d4363 vucomiss xmm2, dword ptr [rbp + rsi*4 - 0x1c]
 cf=std::isnan(_mm_cvtss_f32(x2))||std::isnan(_mm_cvtss_f32(loadv(reinterpret_cast<void*>(rbp + rsi*4 - 0x1c),4)))||_mm_cvtss_f32(x2)<_mm_cvtss_f32(loadv(reinterpret_cast<void*>(rbp + rsi*4 - 0x1c),4)); zf=std::isnan(_mm_cvtss_f32(x2))||std::isnan(_mm_cvtss_f32(loadv(reinterpret_cast<void*>(rbp + rsi*4 - 0x1c),4)))||_mm_cvtss_f32(x2)==_mm_cvtss_f32(loadv(reinterpret_cast<void*>(rbp + rsi*4 - 0x1c),4));
 // 0x13d4369 ja 0x13d44a8
 if(!cf&&!zf) goto L13d44a8;
 // 0x13d436f vmulss xmm4, xmm1, dword ptr [rip + 0x7c9f81]
 x4=_mm_mul_ss(x1,loadv(c1b9e2f8.data(),4));
 // 0x13d4377 vsubss xmm2, xmm14, xmm1
 x2=_mm_sub_ss(x14,x1);
 // 0x13d437b cmp r9d, -1
 zf=r9==static_cast<std::uint32_t>(-1);
 // 0x13d437f vmovss dword ptr [rbp + rsi*4 - 0x28], xmm1
 storev(reinterpret_cast<void*>(rbp + rsi*4 - 0x28),x1,4);
 // 0x13d4385 vmulss xmm3, xmm2, xmm2
 x3=_mm_mul_ss(x2,x2);
 // 0x13d4389 vmulss xmm3, xmm2, xmm3
 x3=_mm_mul_ss(x2,x3);
 // 0x13d438d vmulss xmm6, xmm3, dword ptr [rbp - 0x80]
 x6=_mm_mul_ss(x3,loadv(reinterpret_cast<void*>(rbp - 0x80),4));
 // 0x13d4392 vmulss xmm7, xmm3, dword ptr [rbp - 0x7c]
 x7=_mm_mul_ss(x3,loadv(reinterpret_cast<void*>(rbp - 0x7c),4));
 // 0x13d4397 vmulss xmm3, xmm3, dword ptr [rbp - 0x70]
 x3=_mm_mul_ss(x3,loadv(reinterpret_cast<void*>(rbp - 0x70),4));
 // 0x13d439c vmulss xmm5, xmm4, xmm2
 x5=_mm_mul_ss(x4,x2);
 // 0x13d43a0 vmulss xmm4, xmm1, xmm4
 x4=_mm_mul_ss(x1,x4);
 // 0x13d43a4 vmulss xmm5, xmm2, xmm5
 x5=_mm_mul_ss(x2,x5);
 // 0x13d43a8 vmulss xmm2, xmm2, xmm4
 x2=_mm_mul_ss(x2,x4);
 // 0x13d43ac vmulss xmm4, xmm5, dword ptr [rbp - 0x78]
 x4=_mm_mul_ss(x5,loadv(reinterpret_cast<void*>(rbp - 0x78),4));
 // 0x13d43b1 vaddss xmm4, xmm6, xmm4
 x4=_mm_add_ss(x6,x4);
 // 0x13d43b5 vmulss xmm6, xmm5, dword ptr [rbp - 0x74]
 x6=_mm_mul_ss(x5,loadv(reinterpret_cast<void*>(rbp - 0x74),4));
 // 0x13d43ba vmulss xmm5, xmm5, dword ptr [rbp - 0x6c]
 x5=_mm_mul_ss(x5,loadv(reinterpret_cast<void*>(rbp - 0x6c),4));
 // 0x13d43bf vaddss xmm3, xmm3, xmm5
 x3=_mm_add_ss(x3,x5);
 // 0x13d43c3 vmulss xmm5, xmm2, dword ptr [rbp - 0x68]
 x5=_mm_mul_ss(x2,loadv(reinterpret_cast<void*>(rbp - 0x68),4));
 // 0x13d43c8 vaddss xmm6, xmm7, xmm6
 x6=_mm_add_ss(x7,x6);
 // 0x13d43cc vaddss xmm4, xmm5, xmm4
 x4=_mm_add_ss(x5,x4);
 // 0x13d43d0 vmulss xmm5, xmm2, dword ptr [rbp - 0x64]
 x5=_mm_mul_ss(x2,loadv(reinterpret_cast<void*>(rbp - 0x64),4));
 // 0x13d43d5 vmulss xmm2, xmm2, dword ptr [rbp - 0x60]
 x2=_mm_mul_ss(x2,loadv(reinterpret_cast<void*>(rbp - 0x60),4));
 // 0x13d43da vaddss xmm5, xmm5, xmm6
 x5=_mm_add_ss(x5,x6);
 // 0x13d43de vmulss xmm6, xmm1, xmm1
 x6=_mm_mul_ss(x1,x1);
 // 0x13d43e2 vaddss xmm2, xmm2, xmm3
 x2=_mm_add_ss(x2,x3);
 // 0x13d43e6 vmulss xmm7, xmm1, xmm6
 x7=_mm_mul_ss(x1,x6);
 // 0x13d43ea vmulss xmm3, xmm7, dword ptr [rbp - 0x5c]
 x3=_mm_mul_ss(x7,loadv(reinterpret_cast<void*>(rbp - 0x5c),4));
 // 0x13d43ef vaddss xmm6, xmm3, xmm4
 x6=_mm_add_ss(x3,x4);
 // 0x13d43f3 vmulss xmm3, xmm7, dword ptr [rbp - 0x58]
 x3=_mm_mul_ss(x7,loadv(reinterpret_cast<void*>(rbp - 0x58),4));
 // 0x13d43f8 vmovss dword ptr [rbp - 0x38], xmm6
 storev(reinterpret_cast<void*>(rbp - 0x38),x6,4);
 // 0x13d43fd vaddss xmm5, xmm3, xmm5
 x5=_mm_add_ss(x3,x5);
 // 0x13d4401 vmulss xmm3, xmm7, dword ptr [rbp - 0x54]
 x3=_mm_mul_ss(x7,loadv(reinterpret_cast<void*>(rbp - 0x54),4));
 // 0x13d4406 vmovss dword ptr [rbp - 0x34], xmm5
 storev(reinterpret_cast<void*>(rbp - 0x34),x5,4);
 // 0x13d440b vaddss xmm3, xmm3, xmm2
 x3=_mm_add_ss(x3,x2);
 // 0x13d440f vmovss dword ptr [rbp - 0x30], xmm3
 storev(reinterpret_cast<void*>(rbp - 0x30),x3,4);
 // 0x13d4414 je 0x13d442d
 if(zf) goto L13d442d;
 // 0x13d4416 mov dword ptr [rbp + rcx*4 - 0x38], 0
 storei(reinterpret_cast<void*>(rbp + rcx*4 - 0x38),0,4);
 // 0x13d441e vmovss xmm6, dword ptr [rbp - 0x38]
 x6=loadv(reinterpret_cast<void*>(rbp - 0x38),4);
 // 0x13d4423 vmovss xmm5, dword ptr [rbp - 0x34]
 x5=loadv(reinterpret_cast<void*>(rbp - 0x34),4);
 // 0x13d4428 vmovss xmm3, dword ptr [rbp - 0x30]
 x3=loadv(reinterpret_cast<void*>(rbp - 0x30),4);
L13d442d:;
 // 0x13d442d vmovss xmm2, dword ptr [rbp - 0x48]
 x2=loadv(reinterpret_cast<void*>(rbp - 0x48),4);
 // 0x13d4432 test edx, edx
 zf=(rdx&rdx)==0;cf=false;
 // 0x13d4434 vsubss xmm2, xmm2, xmm6
 x2=_mm_sub_ss(x2,x6);
 // 0x13d4438 vmulss xmm2, xmm2, xmm2
 x2=_mm_mul_ss(x2,x2);
 // 0x13d443c vaddss xmm2, xmm2, dword ptr [rip + 0x7c9ec0]
 x2=_mm_add_ss(x2,loadv(c1b9e304.data(),4));
 // 0x13d4444 vmovss xmm4, dword ptr [rbp - 0x44]
 x4=loadv(reinterpret_cast<void*>(rbp - 0x44),4);
 // 0x13d4449 vsubss xmm4, xmm4, xmm5
 x4=_mm_sub_ss(x4,x5);
 // 0x13d444d vmovss xmm5, dword ptr [rbp - 0x40]
 x5=loadv(reinterpret_cast<void*>(rbp - 0x40),4);
 // 0x13d4452 vmulss xmm4, xmm4, xmm4
 x4=_mm_mul_ss(x4,x4);
 // 0x13d4456 vsubss xmm3, xmm5, xmm3
 x3=_mm_sub_ss(x5,x3);
 // 0x13d445a vaddss xmm2, xmm2, xmm4
 x2=_mm_add_ss(x2,x4);
 // 0x13d445e vmulss xmm3, xmm3, xmm3
 x3=_mm_mul_ss(x3,x3);
 // 0x13d4462 vaddss xmm2, xmm2, xmm3
 x2=_mm_add_ss(x2,x3);
 // 0x13d4466 vmovss dword ptr [rbp + rsi*4 - 0x1c], xmm2
 storev(reinterpret_cast<void*>(rbp + rsi*4 - 0x1c),x2,4);
 // 0x13d446c jne 0x13d41c5
 if(!zf) goto L13d41c5;
 // 0x13d4472 vmovss xmm3, dword ptr [rbp - 0x1c]
 x3=loadv(reinterpret_cast<void*>(rbp - 0x1c),4);
 // 0x13d4477 vmovss xmm7, dword ptr [rbp - 0x18]
 x7=loadv(reinterpret_cast<void*>(rbp - 0x18),4);
 // 0x13d447c vmovss xmm5, dword ptr [rbp - 0x14]
 x5=loadv(reinterpret_cast<void*>(rbp - 0x14),4);
 // 0x13d4481 jmp 0x13d44a8
 goto L13d44a8;
L13d4483:;
 // 0x13d4483 test rsi, r10
 zf=(rsi&r10)==0;cf=false;
 // 0x13d4486 jne 0x13d4490
 if(!zf) goto L13d4490;
 // 0x13d4488 test esi, esi
 zf=(rsi&rsi)==0;cf=false;
 // 0x13d448a je 0x13d42a5
 if(zf) goto L13d42a5;
L13d4490:;
 // 0x13d4490 vmovaps xmm7, xmmword ptr [rbp - 0xc0]
 x7=loadv(reinterpret_cast<void*>(rbp - 0xc0),16);
 // 0x13d4498 vmovaps xmm5, xmmword ptr [rbp - 0xb0]
 x5=loadv(reinterpret_cast<void*>(rbp - 0xb0),16);
 // 0x13d44a0 vmovaps xmm3, xmmword ptr [rbp - 0xa0]
 x3=loadv(reinterpret_cast<void*>(rbp - 0xa0),16);
L13d44a8:;
 // 0x13d44a8 xor eax, eax
 rax=0;zf=true;cf=false;
 // 0x13d44aa vucomiss xmm3, xmm7
 cf=std::isnan(_mm_cvtss_f32(x3))||std::isnan(_mm_cvtss_f32(x7))||_mm_cvtss_f32(x3)<_mm_cvtss_f32(x7); zf=std::isnan(_mm_cvtss_f32(x3))||std::isnan(_mm_cvtss_f32(x7))||_mm_cvtss_f32(x3)==_mm_cvtss_f32(x7);
 // 0x13d44ae mov ecx, 2
 rcx=static_cast<std::uint32_t>(2);
 // 0x13d44b3 seta al
 rax=(!cf&&!zf)?1:0;
 // 0x13d44b6 vmovss xmm1, dword ptr [rbp + rax*4 - 0x1c]
 x1=loadv(reinterpret_cast<void*>(rbp + rax*4 - 0x1c),4);
 // 0x13d44bc vucomiss xmm1, xmm5
 cf=std::isnan(_mm_cvtss_f32(x1))||std::isnan(_mm_cvtss_f32(x5))||_mm_cvtss_f32(x1)<_mm_cvtss_f32(x5); zf=std::isnan(_mm_cvtss_f32(x1))||std::isnan(_mm_cvtss_f32(x5))||_mm_cvtss_f32(x1)==_mm_cvtss_f32(x5);
 // 0x13d44c0 cmovbe rcx, rax
 if(cf||zf) rcx=rax;
 // 0x13d44c4 vmovss xmm1, dword ptr [rbp + rcx*4 - 0x28]
 x1=loadv(reinterpret_cast<void*>(rbp + rcx*4 - 0x28),4);
 // 0x13d44ca vsubss xmm0, xmm1, xmm0
 x0=_mm_sub_ss(x1,x0);
 // 0x13d44ce vdivss xmm0, xmm0, dword ptr [rbp - 0x3c]
 x0=_mm_div_ss(x0,loadv(reinterpret_cast<void*>(rbp - 0x3c),4));
 // 0x13d44d3 vmulss xmm0, xmm15, xmm0
 x0=_mm_mul_ss(x15,x0);
 // 0x13d44d7 vmovss dword ptr [r8], xmm0
 storev(reinterpret_cast<void*>(r8),x0,4);
 // 0x13d44dc vmovss xmm0, dword ptr [rbp + rcx*4 - 0x1c]
 x0=loadv(reinterpret_cast<void*>(rbp + rcx*4 - 0x1c),4);
 // 0x13d44e2 jmp 0x13d3f0c
 goto L13d3f0c;
}
}
