#include "ctr/core/matrix_adjugate.h"
#include <array>
#include <cstring>
#include <immintrin.h>
namespace {
__m128 load_vector(const void*p,std::size_t n){__m128 v=_mm_setzero_ps();std::memcpy(&v,p,n);return v;}
void store_vector(void*p,__m128 v,std::size_t n){std::memcpy(p,&v,n);}
}
namespace ctr {
void matrix_adjugate(std::span<const float,16> input,std::span<float,16> output){
 const auto* input_bytes=reinterpret_cast<const std::byte*>(input.data());
 auto* output_bytes=reinterpret_cast<std::byte*>(output.data());
 alignas(16) std::array<std::byte,512> scratch{};
 const std::array<std::uint32_t,4> constant_1bac640={0x0U,0x80000000U,0x0U,0x80000000U};
 const std::array<std::uint32_t,4> constant_1bac650={0x80000000U,0x0U,0x80000000U,0x0U};
 __m128 x0=_mm_setzero_ps(),x1=_mm_setzero_ps(),x2=_mm_setzero_ps(),x3=_mm_setzero_ps(),x4=_mm_setzero_ps(),x5=_mm_setzero_ps(),x6=_mm_setzero_ps(),x7=_mm_setzero_ps(),x8=_mm_setzero_ps(),x9=_mm_setzero_ps(),x10=_mm_setzero_ps(),x11=_mm_setzero_ps(),x12=_mm_setzero_ps(),x13=_mm_setzero_ps(),x14=_mm_setzero_ps(),x15=_mm_setzero_ps();
 // 0x13cb757 vmovss xmm6, dword ptr [rdi + 0x20]
 x6=load_vector(input_bytes+32,4);
 // 0x13cb75c vmovss xmm1, dword ptr [rdi + 0x24]
 x1=load_vector(input_bytes+36,4);
 // 0x13cb761 vmovss xmm10, dword ptr [rdi + 0x34]
 x10=load_vector(input_bytes+52,4);
 // 0x13cb766 vmovss xmm0, dword ptr [rdi + 0x3c]
 x0=load_vector(input_bytes+60,4);
 // 0x13cb76b vmovss xmm14, dword ptr [rdi + 0x14]
 x14=load_vector(input_bytes+20,4);
 // 0x13cb770 vmovss xmm11, dword ptr [rdi + 0x10]
 x11=load_vector(input_bytes+16,4);
 // 0x13cb775 vmovss xmm13, dword ptr [rdi + 0x1c]
 x13=load_vector(input_bytes+28,4);
 // 0x13cb77a vmovss xmm7, dword ptr [rdi + 0x28]
 x7=load_vector(input_bytes+40,4);
 // 0x13cb77f vmovss xmm2, dword ptr [rdi + 0x38]
 x2=load_vector(input_bytes+56,4);
 // 0x13cb784 vmovss xmm8, dword ptr [rdi + 0x18]
 x8=load_vector(input_bytes+24,4);
 // 0x13cb789 vmovss xmm15, dword ptr [rdi + 0x2c]
 x15=load_vector(input_bytes+44,4);
 // 0x13cb78e vmovss xmm9, dword ptr [rdi + 0x30]
 x9=load_vector(input_bytes+48,4);
 // 0x13cb793 vinsertps xmm3, xmm1, xmm6, 0x10
 x3=_mm_insert_ps(x1,x6,0x10);
 // 0x13cb799 vinsertps xmm4, xmm0, xmm10, 0x10
 x4=_mm_insert_ps(x0,x10,0x10);
 // 0x13cb79f vinsertps xmm5, xmm10, xmm1, 0x10
 x5=_mm_insert_ps(x10,x1,0x10);
 // 0x13cb7a5 vmovaps xmmword ptr [rsp - 0x50], xmm13
 store_vector(scratch.data()+128-80,x13,16);
 // 0x13cb7ab vmovaps xmmword ptr [rsp - 0x70], xmm1
 store_vector(scratch.data()+128-112,x1,16);
 // 0x13cb7b1 vmovaps xmmword ptr [rsp], xmm15
 store_vector(scratch.data()+128+0,x15,16);
 // 0x13cb7b6 vmovaps xmmword ptr [rsp - 0x60], xmm11
 store_vector(scratch.data()+128-96,x11,16);
 // 0x13cb7bc vmovaps xmmword ptr [rsp - 0x30], xmm7
 store_vector(scratch.data()+128-48,x7,16);
 // 0x13cb7c2 vmovaps xmmword ptr [rsp + 0x50], xmm6
 store_vector(scratch.data()+128+80,x6,16);
 // 0x13cb7c8 vinsertps xmm3, xmm3, xmm14, 0x20
 x3=_mm_insert_ps(x3,x14,0x20);
 // 0x13cb7ce vinsertps xmm4, xmm4, xmm0, 0x20
 x4=_mm_insert_ps(x4,x0,0x20);
 // 0x13cb7d4 vinsertps xmm5, xmm5, xmm13, 0x20
 x5=_mm_insert_ps(x5,x13,0x20);
 // 0x13cb7da vinsertps xmm3, xmm3, xmm11, 0x30
 x3=_mm_insert_ps(x3,x11,0x30);
 // 0x13cb7e0 vinsertps xmm4, xmm4, xmm1, 0x30
 x4=_mm_insert_ps(x4,x1,0x30);
 // 0x13cb7e6 vinsertps xmm5, xmm5, xmm6, 0x30
 x5=_mm_insert_ps(x5,x6,0x30);
 // 0x13cb7ec vmulps xmm3, xmm3, xmm4
 x3=_mm_mul_ps(x3,x4);
 // 0x13cb7f0 vmovsd xmm4, qword ptr [rdi + 0x2c]
 x4=load_vector(input_bytes+44,8);
 // 0x13cb7f5 vinsertps xmm4, xmm4, xmm10, 0x20
 x4=_mm_insert_ps(x4,x10,0x20);
 // 0x13cb7fb vinsertps xmm4, xmm4, xmm14, 0x30
 x4=_mm_insert_ps(x4,x14,0x30);
 // 0x13cb801 vmulps xmm4, xmm4, xmm5
 x4=_mm_mul_ps(x4,x5);
 // 0x13cb805 vinsertps xmm5, xmm2, xmm10, 0x10
 x5=_mm_insert_ps(x2,x10,0x10);
 // 0x13cb80b vinsertps xmm5, xmm5, xmm2, 0x20
 x5=_mm_insert_ps(x5,x2,0x20);
 // 0x13cb811 vsubps xmm12, xmm3, xmm4
 x12=_mm_sub_ps(x3,x4);
 // 0x13cb815 vinsertps xmm3, xmm7, xmm1, 0x10
 x3=_mm_insert_ps(x7,x1,0x10);
 // 0x13cb81b vinsertps xmm4, xmm0, xmm2, 0x10
 x4=_mm_insert_ps(x0,x2,0x10);
 // 0x13cb821 vinsertps xmm5, xmm5, xmm1, 0x30
 x5=_mm_insert_ps(x5,x1,0x30);
 // 0x13cb827 vinsertps xmm3, xmm3, xmm8, 0x20
 x3=_mm_insert_ps(x3,x8,0x20);
 // 0x13cb82d vinsertps xmm4, xmm4, xmm0, 0x20
 x4=_mm_insert_ps(x4,x0,0x20);
 // 0x13cb833 vinsertps xmm3, xmm3, xmm14, 0x30
 x3=_mm_insert_ps(x3,x14,0x30);
 // 0x13cb839 vinsertps xmm4, xmm4, xmm7, 0x30
 x4=_mm_insert_ps(x4,x7,0x30);
 // 0x13cb83f vmulps xmm3, xmm3, xmm4
 x3=_mm_mul_ps(x3,x4);
 // 0x13cb843 vinsertps xmm4, xmm15, xmm7, 0x10
 x4=_mm_insert_ps(x15,x7,0x10);
 // 0x13cb849 vmovaps xmmword ptr [rsp - 0x40], xmm12
 store_vector(scratch.data()+128-64,x12,16);
 // 0x13cb84f vinsertps xmm4, xmm4, xmm13, 0x20
 x4=_mm_insert_ps(x4,x13,0x20);
 // 0x13cb855 vinsertps xmm4, xmm4, xmm8, 0x30
 x4=_mm_insert_ps(x4,x8,0x30);
 // 0x13cb85b vmulps xmm4, xmm4, xmm5
 x4=_mm_mul_ps(x4,x5);
 // 0x13cb85f vmovss xmm5, dword ptr [rdi + 4]
 x5=load_vector(input_bytes+4,4);
 // 0x13cb864 vsubps xmm13, xmm3, xmm4
 x13=_mm_sub_ps(x3,x4);
 // 0x13cb868 vmulss xmm4, xmm15, xmm9
 x4=_mm_mul_ss(x15,x9);
 // 0x13cb86d vmulss xmm3, xmm6, xmm0
 x3=_mm_mul_ss(x6,x0);
 // 0x13cb871 vmovaps xmm15, xmm8
 x15=x8;
 // 0x13cb876 vsubss xmm3, xmm3, xmm4
 x3=_mm_sub_ss(x3,x4);
 // 0x13cb87a vmulss xmm1, xmm11, xmm13
 x1=_mm_mul_ss(x11,x13);
 // 0x13cb87f vmulss xmm4, xmm8, xmm3
 x4=_mm_mul_ss(x8,x3);
 // 0x13cb883 vmovaps xmm8, xmm3
 x8=x3;
 // 0x13cb887 vmovss dword ptr [rsp - 0x78], xmm8
 store_vector(scratch.data()+128-120,x8,4);
 // 0x13cb88d vsubss xmm3, xmm1, xmm4
 x3=_mm_sub_ss(x1,x4);
 // 0x13cb891 vmulss xmm4, xmm7, xmm9
 x4=_mm_mul_ss(x7,x9);
 // 0x13cb896 vmovaps xmm1, xmm9
 x1=x9;
 // 0x13cb89a vmovaps xmmword ptr [rsp + 0x70], xmm3
 store_vector(scratch.data()+128+112,x3,16);
 // 0x13cb8a0 vmulss xmm3, xmm6, xmm2
 x3=_mm_mul_ss(x6,x2);
 // 0x13cb8a4 vmulss xmm6, xmm15, xmm10
 x6=_mm_mul_ss(x15,x10);
 // 0x13cb8a9 vsubss xmm4, xmm3, xmm4
 x4=_mm_sub_ss(x3,x4);
 // 0x13cb8ad vmovshdup xmm3, xmm13
 x3=_mm_movehdup_ps(x13);
 // 0x13cb8b2 vmovaps xmmword ptr [rsp + 0x20], xmm3
 store_vector(scratch.data()+128+32,x3,16);
 // 0x13cb8b8 vmulss xmm3, xmm11, xmm3
 x3=_mm_mul_ss(x11,x3);
 // 0x13cb8bc vmovss xmm11, dword ptr [rdi + 8]
 x11=load_vector(input_bytes+8,4);
 // 0x13cb8c1 vmovaps xmmword ptr [rsp + 0x30], xmm4
 store_vector(scratch.data()+128+48,x4,16);
 // 0x13cb8c7 vmulss xmm4, xmm14, xmm4
 x4=_mm_mul_ss(x14,x4);
 // 0x13cb8cb vsubss xmm3, xmm3, xmm4
 x3=_mm_sub_ss(x3,x4);
 // 0x13cb8cf vmovss xmm4, dword ptr [rdi]
 x4=load_vector(input_bytes+0,4);
 // 0x13cb8d3 vmulss xmm7, xmm11, xmm12
 x7=_mm_mul_ss(x11,x12);
 // 0x13cb8d8 vmovaps xmmword ptr [rsp + 0x90], xmm3
 store_vector(scratch.data()+128+144,x3,16);
 // 0x13cb8e1 vmulss xmm3, xmm5, xmm13
 x3=_mm_mul_ss(x5,x13);
 // 0x13cb8e6 vsubss xmm3, xmm3, xmm7
 x3=_mm_sub_ss(x3,x7);
 // 0x13cb8ea vmulss xmm7, xmm4, xmm12
 x7=_mm_mul_ss(x4,x12);
 // 0x13cb8ef vmulss xmm12, xmm5, xmm8
 x12=_mm_mul_ss(x5,x8);
 // 0x13cb8f4 vmovaps xmm8, xmmword ptr [rsp - 0x50]
 x8=load_vector(scratch.data()+128-80,16);
 // 0x13cb8fa vmovaps xmmword ptr [rsp - 0x10], xmm3
 store_vector(scratch.data()+128-16,x3,16);
 // 0x13cb900 vsubss xmm3, xmm7, xmm12
 x3=_mm_sub_ss(x7,x12);
 // 0x13cb905 vmulss xmm7, xmm14, xmm2
 x7=_mm_mul_ss(x14,x2);
 // 0x13cb909 vsubss xmm9, xmm7, xmm6
 x9=_mm_sub_ss(x7,x6);
 // 0x13cb90d vmovaps xmm7, xmmword ptr [rsp - 0x60]
 x7=load_vector(scratch.data()+128-96,16);
 // 0x13cb913 vmovaps xmmword ptr [rsp + 0x80], xmm3
 store_vector(scratch.data()+128+128,x3,16);
 // 0x13cb91c vmovaps xmm3, xmm1
 x3=x1;
 // 0x13cb920 vmulss xmm6, xmm8, xmm3
 x6=_mm_mul_ss(x8,x3);
 // 0x13cb924 vmulss xmm0, xmm7, xmm0
 x0=_mm_mul_ss(x7,x0);
 // 0x13cb928 vsubss xmm1, xmm0, xmm6
 x1=_mm_sub_ss(x0,x6);
 // 0x13cb92c vpermilpd xmm0, xmm13, 1
 x0=_mm_castpd_ps(_mm_shuffle_pd(_mm_castps_pd(x13),_mm_castps_pd(x13),1));
 // 0x13cb932 vmulss xmm0, xmm4, xmm0
 x0=_mm_mul_ss(x4,x0);
 // 0x13cb936 vmulss xmm6, xmm11, xmm1
 x6=_mm_mul_ss(x11,x1);
 // 0x13cb93a vmovss dword ptr [rsp - 0x74], xmm1
 store_vector(scratch.data()+128-116,x1,4);
 // 0x13cb940 vmulss xmm1, xmm15, xmm3
 x1=_mm_mul_ss(x15,x3);
 // 0x13cb944 vsubss xmm0, xmm0, xmm6
 x0=_mm_sub_ss(x0,x6);
 // 0x13cb948 vmovaps xmmword ptr [rsp + 0x10], xmm0
 store_vector(scratch.data()+128+16,x0,16);
 // 0x13cb94e vmulss xmm0, xmm7, xmm2
 x0=_mm_mul_ss(x7,x2);
 // 0x13cb952 vmovaps xmm2, xmm3
 x2=x3;
 // 0x13cb956 vmovaps xmm3, xmm15
 x3=x15;
 // 0x13cb95a vmulss xmm2, xmm14, xmm2
 x2=_mm_mul_ss(x14,x2);
 // 0x13cb95e vmovaps xmmword ptr [rsp - 0x20], xmm3
 store_vector(scratch.data()+128-32,x3,16);
 // 0x13cb964 vsubss xmm12, xmm0, xmm1
 x12=_mm_sub_ss(x0,x1);
 // 0x13cb968 vmulss xmm1, xmm7, xmm10
 x1=_mm_mul_ss(x7,x10);
 // 0x13cb96d vmovaps xmm10, xmmword ptr [rsp]
 x10=load_vector(scratch.data()+128+0,16);
 // 0x13cb972 vsubss xmm0, xmm1, xmm2
 x0=_mm_sub_ss(x1,x2);
 // 0x13cb976 vmulss xmm2, xmm4, xmm9
 x2=_mm_mul_ss(x4,x9);
 // 0x13cb97b vmulss xmm6, xmm5, xmm12
 x6=_mm_mul_ss(x5,x12);
 // 0x13cb980 vmovaps xmmword ptr [rsp + 0x40], xmm0
 store_vector(scratch.data()+128+64,x0,16);
 // 0x13cb986 vsubss xmm0, xmm2, xmm6
 x0=_mm_sub_ss(x2,x6);
 // 0x13cb98a vmulss xmm6, xmm8, dword ptr [rsp - 0x30]
 x6=_mm_mul_ss(x8,load_vector(scratch.data()+128-48,4));
 // 0x13cb990 vmulss xmm2, xmm3, xmm10
 x2=_mm_mul_ss(x3,x10);
 // 0x13cb995 vmovaps xmmword ptr [rsp + 0x60], xmm0
 store_vector(scratch.data()+128+96,x0,16);
 // 0x13cb99b vsubss xmm15, xmm2, xmm6
 x15=_mm_sub_ss(x2,x6);
 // 0x13cb99f vmulss xmm2, xmm8, dword ptr [rsp - 0x70]
 x2=_mm_mul_ss(x8,load_vector(scratch.data()+128-112,4));
 // 0x13cb9a5 vmulss xmm6, xmm14, xmm10
 x6=_mm_mul_ss(x14,x10);
 // 0x13cb9aa vsubss xmm0, xmm6, xmm2
 x0=_mm_sub_ss(x6,x2);
 // 0x13cb9ae vmulss xmm2, xmm5, xmm15
 x2=_mm_mul_ss(x5,x15);
 // 0x13cb9b3 vmulss xmm6, xmm11, xmm0
 x6=_mm_mul_ss(x11,x0);
 // 0x13cb9b7 vmovss dword ptr [rsp - 0x70], xmm0
 store_vector(scratch.data()+128-112,x0,4);
 // 0x13cb9bd vmovaps xmm0, xmmword ptr [rsp - 0x40]
 x0=load_vector(scratch.data()+128-64,16);
 // 0x13cb9c3 vsubss xmm2, xmm2, xmm6
 x2=_mm_sub_ss(x2,x6);
 // 0x13cb9c7 vmulss xmm6, xmm3, xmm0
 x6=_mm_mul_ss(x3,x0);
 // 0x13cb9cb vinsertps xmm6, xmm6, dword ptr [rsp - 0x10], 0x10
 x6=_mm_insert_ps(x6,load_vector(scratch.data()+128-16,4),0x10);
 // 0x13cb9d3 vpermilpd xmm7, xmm0, 1
 x7=_mm_castpd_ps(_mm_shuffle_pd(_mm_castps_pd(x0),_mm_castps_pd(x0),1));
 // 0x13cb9d9 vmovaps xmm3, xmmword ptr [rsp + 0x30]
 x3=load_vector(scratch.data()+128+48,16);
 // 0x13cb9df vmulss xmm7, xmm11, xmm7
 x7=_mm_mul_ss(x11,x7);
 // 0x13cb9e3 vinsertps xmm6, xmm6, xmm7, 0x20
 x6=_mm_insert_ps(x6,x7,0x20);
 // 0x13cb9e9 vinsertps xmm6, xmm6, xmm2, 0x30
 x6=_mm_insert_ps(x6,x2,0x30);
 // 0x13cb9ef vmovss xmm2, dword ptr [rdi + 0xc]
 x2=load_vector(input_bytes+12,4);
 // 0x13cb9f4 vinsertps xmm7, xmm14, xmm2, 0x10
 x7=_mm_insert_ps(x14,x2,0x10);
 // 0x13cb9fa vmulss xmm0, xmm2, xmm9
 x0=_mm_mul_ss(x2,x9);
 // 0x13cb9ff vmovaps xmm9, xmmword ptr [rsp - 0x60]
 x9=load_vector(scratch.data()+128-96,16);
 // 0x13cba05 vinsertps xmm7, xmm7, xmm5, 0x20
 x7=_mm_insert_ps(x7,x5,0x20);
 // 0x13cba0b vinsertps xmm7, xmm7, xmm2, 0x30
 x7=_mm_insert_ps(x7,x2,0x30);
 // 0x13cba11 vmulps xmm7, xmm7, xmm13
 x7=_mm_mul_ps(x7,x13);
 // 0x13cba16 vaddsubps xmm7, xmm7, xmm6
 x7=_mm_addsub_ps(x7,x6);
 // 0x13cba1a vmulss xmm6, xmm8, dword ptr [rsp + 0x20]
 x6=_mm_mul_ss(x8,load_vector(scratch.data()+128+32,4));
 // 0x13cba20 vblendps xmm6, xmm6, xmmword ptr [rip + 0x7e0c16], 0xe
 x6=_mm_blend_ps(x6,load_vector(constant_1bac640.data(),16),0xe);
 // 0x13cba2a vinsertps xmm0, xmm6, xmm0, 0x20
 x0=_mm_insert_ps(x6,x0,0x20);
 // 0x13cba30 vaddps xmm6, xmm0, xmm7
 x6=_mm_add_ps(x0,x7);
 // 0x13cba34 vsubps xmm0, xmm0, xmm7
 x0=_mm_sub_ps(x0,x7);
 // 0x13cba38 vblendps xmm0, xmm6, xmm0, 0xa
 x0=_mm_blend_ps(x6,x0,0xa);
 // 0x13cba3e vmovaps xmm6, xmm8
 x6=x8;
 // 0x13cba42 vmulss xmm8, xmm9, xmm10
 x8=_mm_mul_ss(x9,x10);
 // 0x13cba47 vmovaps xmm10, xmmword ptr [rsp + 0x50]
 x10=load_vector(scratch.data()+128+80,16);
 // 0x13cba4d vinsertps xmm1, xmm6, xmm4, 0x10
 x1=_mm_insert_ps(x6,x4,0x10);
 // 0x13cba53 vmovups xmmword ptr [rsi], xmm0
 store_vector(output_bytes+0,x0,16);
 // 0x13cba57 vinsertps xmm0, xmm3, xmm13, 0x10
 x0=_mm_insert_ps(x3,x13,0x10);
 // 0x13cba5d vinsertps xmm0, xmm0, xmm12, 0x20
 x0=_mm_insert_ps(x0,x12,0x20);
 // 0x13cba63 vinsertps xmm1, xmm1, xmm2, 0x20
 x1=_mm_insert_ps(x1,x2,0x20);
 // 0x13cba69 vinsertps xmm0, xmm0, xmm15, 0x30
 x0=_mm_insert_ps(x0,x15,0x30);
 // 0x13cba6f vinsertps xmm1, xmm1, xmm4, 0x30
 x1=_mm_insert_ps(x1,x4,0x30);
 // 0x13cba75 vmulps xmm0, xmm1, xmm0
 x0=_mm_mul_ps(x1,x0);
 // 0x13cba79 vmovups xmm1, xmmword ptr [rip + 0x7e0bcf]
 x1=load_vector(constant_1bac650.data(),16);
 // 0x13cba81 vmulss xmm6, xmm6, xmm10
 x6=_mm_mul_ss(x6,x10);
 // 0x13cba86 vsubss xmm8, xmm8, xmm6
 x8=_mm_sub_ss(x8,x6);
 // 0x13cba8a vmovss xmm6, dword ptr [rsp - 0x78]
 x6=load_vector(scratch.data()+128-120,4);
 // 0x13cba90 vmulss xmm15, xmm14, xmm6
 x15=_mm_mul_ss(x14,x6);
 // 0x13cba94 vmulss xmm7, xmm11, xmm6
 x7=_mm_mul_ss(x11,x6);
 // 0x13cba98 vmovaps xmm6, xmmword ptr [rsp + 0x70]
 x6=load_vector(scratch.data()+128+112,16);
 // 0x13cba9e vmovaps xmm14, xmmword ptr [rsp - 0x20]
 x14=load_vector(scratch.data()+128-32,16);
 // 0x13cbaa4 vinsertps xmm7, xmm6, xmm7, 0x10
 x7=_mm_insert_ps(x6,x7,0x10);
 // 0x13cbaaa vinsertps xmm7, xmm7, dword ptr [rsp + 0x10], 0x20
 x7=_mm_insert_ps(x7,load_vector(scratch.data()+128+16,4),0x20);
 // 0x13cbab2 vmulss xmm6, xmm11, xmm8
 x6=_mm_mul_ss(x11,x8);
 // 0x13cbab7 vinsertps xmm6, xmm7, xmm6, 0x30
 x6=_mm_insert_ps(x7,x6,0x30);
 // 0x13cbabd vaddps xmm7, xmm0, xmm6
 x7=_mm_add_ps(x0,x6);
 // 0x13cbac1 vsubps xmm0, xmm0, xmm6
 x0=_mm_sub_ps(x0,x6);
 // 0x13cbac5 vblendps xmm6, xmm7, xmm0, 0xa
 x6=_mm_blend_ps(x7,x0,0xa);
 // 0x13cbacb vmulss xmm0, xmm9, dword ptr [rsp - 0x30]
 x0=_mm_mul_ss(x9,load_vector(scratch.data()+128-48,4));
 // 0x13cbad1 vmulss xmm7, xmm14, xmm10
 x7=_mm_mul_ss(x14,x10);
 // 0x13cbad6 vsubss xmm10, xmm0, xmm7
 x10=_mm_sub_ss(x0,x7);
 // 0x13cbada vmulss xmm0, xmm2, xmm3
 x0=_mm_mul_ss(x2,x3);
 // 0x13cbade vinsertps xmm0, xmm1, xmm0, 0x10
 x0=_mm_insert_ps(x1,x0,0x10);
 // 0x13cbae4 vmulss xmm1, xmm5, xmm8
 x1=_mm_mul_ss(x5,x8);
 // 0x13cbae9 vmulss xmm12, xmm2, xmm10
 x12=_mm_mul_ss(x2,x10);
 // 0x13cbaee vinsertps xmm0, xmm0, xmm12, 0x30
 x0=_mm_insert_ps(x0,x12,0x30);
 // 0x13cbaf4 vaddsubps xmm0, xmm0, xmm6
 x0=_mm_addsub_ps(x0,x6);
 // 0x13cbaf8 vmulss xmm6, xmm5, dword ptr [rsp - 0x74]
 x6=_mm_mul_ss(x5,load_vector(scratch.data()+128-116,4));
 // 0x13cbafe vmovups xmmword ptr [rsi + 0x10], xmm0
 store_vector(output_bytes+16,x0,16);
 // 0x13cbb03 vmulss xmm0, xmm4, dword ptr [rsp - 0x70]
 x0=_mm_mul_ss(x4,load_vector(scratch.data()+128-112,4));
 // 0x13cbb09 vsubss xmm0, xmm0, xmm1
 x0=_mm_sub_ss(x0,x1);
 // 0x13cbb0d vinsertps xmm1, xmm15, dword ptr [rsp + 0x80], 0x10
 x1=_mm_insert_ps(x15,load_vector(scratch.data()+128+128,4),0x10);
 // 0x13cbb18 vinsertps xmm1, xmm1, xmm6, 0x20
 x1=_mm_insert_ps(x1,x6,0x20);
 // 0x13cbb1e vmovaps xmm6, xmmword ptr [rsp - 0x40]
 x6=load_vector(scratch.data()+128-64,16);
 // 0x13cbb24 vinsertps xmm0, xmm1, xmm0, 0x30
 x0=_mm_insert_ps(x1,x0,0x30);
 // 0x13cbb2a vinsertps xmm1, xmm9, xmm2, 0x10
 x1=_mm_insert_ps(x9,x2,0x10);
 // 0x13cbb30 vinsertps xmm1, xmm1, xmm4, 0x20
 x1=_mm_insert_ps(x1,x4,0x20);
 // 0x13cbb36 vinsertps xmm1, xmm1, xmm2, 0x30
 x1=_mm_insert_ps(x1,x2,0x30);
 // 0x13cbb3c vmulps xmm1, xmm1, xmm6
 x1=_mm_mul_ps(x1,x6);
 // 0x13cbb40 vmovshdup xmm8, xmm6
 x8=_mm_movehdup_ps(x6);
 // 0x13cbb44 vmovaps xmm7, xmm6
 x7=x6;
 // 0x13cbb48 vmulss xmm6, xmm8, dword ptr [rsp - 0x50]
 x6=_mm_mul_ss(x8,load_vector(scratch.data()+128-80,4));
 // 0x13cbb4e vaddsubps xmm0, xmm1, xmm0
 x0=_mm_addsub_ps(x1,x0);
 // 0x13cbb52 vmovaps xmm1, xmmword ptr [rsp + 0x40]
 x1=load_vector(scratch.data()+128+64,16);
 // 0x13cbb58 vblendps xmm6, xmm6, xmmword ptr [rip + 0x7e0ade], 0xe
 x6=_mm_blend_ps(x6,load_vector(constant_1bac640.data(),16),0xe);
 // 0x13cbb62 vmulss xmm2, xmm2, xmm1
 x2=_mm_mul_ss(x2,x1);
 // 0x13cbb66 vinsertps xmm2, xmm6, xmm2, 0x20
 x2=_mm_insert_ps(x6,x2,0x20);
 // 0x13cbb6c vaddps xmm6, xmm2, xmm0
 x6=_mm_add_ps(x2,x0);
 // 0x13cbb70 vsubps xmm0, xmm2, xmm0
 x0=_mm_sub_ps(x2,x0);
 // 0x13cbb74 vinsertps xmm2, xmm13, xmm7, 0x4c
 x2=_mm_insert_ps(x13,x7,0x4c);
 // 0x13cbb7a vinsertps xmm2, xmm2, xmm1, 0x20
 x2=_mm_insert_ps(x2,x1,0x20);
 // 0x13cbb80 vmovaps xmm1, xmmword ptr [rsp + 0x90]
 x1=load_vector(scratch.data()+128+144,16);
 // 0x13cbb89 vblendps xmm2, xmm2, xmm13, 8
 x2=_mm_blend_ps(x2,x13,8);
 // 0x13cbb8f vblendps xmm0, xmm6, xmm0, 0xa
 x0=_mm_blend_ps(x6,x0,0xa);
 // 0x13cbb95 vmovups xmmword ptr [rsi + 0x20], xmm0
 store_vector(output_bytes+32,x0,16);
 // 0x13cbb9a vinsertps xmm0, xmm14, xmm4, 0x10
 x0=_mm_insert_ps(x14,x4,0x10);
 // 0x13cbba0 vinsertps xmm0, xmm0, xmm11, 0x20
 x0=_mm_insert_ps(x0,x11,0x20);
 // 0x13cbba6 vinsertps xmm0, xmm0, xmm4, 0x30
 x0=_mm_insert_ps(x0,x4,0x30);
 // 0x13cbbac vmovups xmm4, xmmword ptr [rip + 0x7e0a9c]
 x4=load_vector(constant_1bac650.data(),16);
 // 0x13cbbb4 vmulps xmm0, xmm0, xmm2
 x0=_mm_mul_ps(x0,x2);
 // 0x13cbbb8 vmulss xmm2, xmm5, xmm3
 x2=_mm_mul_ss(x5,x3);
 // 0x13cbbbc vmulss xmm3, xmm5, xmm10
 x3=_mm_mul_ss(x5,x10);
 // 0x13cbbc1 vinsertps xmm2, xmm1, xmm2, 0x10
 x2=_mm_insert_ps(x1,x2,0x10);
 // 0x13cbbc7 vinsertps xmm2, xmm2, dword ptr [rsp + 0x60], 0x20
 x2=_mm_insert_ps(x2,load_vector(scratch.data()+128+96,4),0x20);
 // 0x13cbbcf vmulss xmm1, xmm11, xmm8
 x1=_mm_mul_ss(x11,x8);
 // 0x13cbbd4 vinsertps xmm1, xmm4, xmm1, 0x10
 x1=_mm_insert_ps(x4,x1,0x10);
 // 0x13cbbda vinsertps xmm2, xmm2, xmm3, 0x30
 x2=_mm_insert_ps(x2,x3,0x30);
 // 0x13cbbe0 vaddps xmm3, xmm0, xmm2
 x3=_mm_add_ps(x0,x2);
 // 0x13cbbe4 vsubps xmm0, xmm0, xmm2
 x0=_mm_sub_ps(x0,x2);
 // 0x13cbbe8 vpermilps xmm2, xmm7, 0xe7
 x2=_mm_shuffle_ps(x7,x7,0xe7);
 // 0x13cbbee vmulss xmm2, xmm11, xmm2
 x2=_mm_mul_ss(x11,x2);
 // 0x13cbbf2 vblendps xmm0, xmm3, xmm0, 0xa
 x0=_mm_blend_ps(x3,x0,0xa);
 // 0x13cbbf8 vinsertps xmm1, xmm1, xmm2, 0x30
 x1=_mm_insert_ps(x1,x2,0x30);
 // 0x13cbbfe vaddsubps xmm0, xmm1, xmm0
 x0=_mm_addsub_ps(x1,x0);
 // 0x13cbc02 vmovups xmmword ptr [rsi + 0x30], xmm0
 store_vector(output_bytes+48,x0,16);
}
}
