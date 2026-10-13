// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "original_call.hpp"
#include <array>
namespace ctrport {
enum class XmmArgumentKind { Float32, Float64 };
enum class OriginalReturnKind { Integer, Float32, Float64 };
struct OriginalRegisterArguments {
    std::array<std::uint64_t,6> integers{};
    std::array<std::uint64_t,8> xmm_bits{};
    std::array<XmmArgumentKind,8> xmm_kinds{};
    unsigned integer_count=0,xmm_count=0;
    OriginalReturnKind return_kind=OriginalReturnKind::Integer;
};
struct OriginalRegisterResult {std::uint64_t integer=0,xmm_bits=0;};
extern "C" std::uint64_t ctr_call_original_registers(void*,const std::uint64_t*,const std::uint64_t*,std::uint64_t*);
inline void validate_register_arguments(const OriginalRegisterArguments& args,unsigned declared_integer_count,unsigned declared_xmm_count) {
    if(args.integer_count>6 || args.xmm_count>8 || args.integer_count!=declared_integer_count || args.xmm_count!=declared_xmm_count) throw std::runtime_error("Original register count differs from declaration");
    for(unsigned i=0;i<args.xmm_count;++i) {
        if(args.xmm_kinds[i]!=XmmArgumentKind::Float32 && args.xmm_kinds[i]!=XmmArgumentKind::Float64) throw std::runtime_error("Original XMM type invalid");
        if(args.xmm_kinds[i]==XmmArgumentKind::Float32 && args.xmm_bits[i]>>32) throw std::runtime_error("Original float32 payload exceeds32bits");
    }
    if(args.return_kind!=OriginalReturnKind::Integer && args.return_kind!=OriginalReturnKind::Float32 && args.return_kind!=OriginalReturnKind::Float64) throw std::runtime_error("Original return type invalid");
}
inline OriginalRegisterResult invoke_original_registers(void* target,const OriginalRegisterArguments& args,unsigned integer_count,unsigned xmm_count) {
    validate_register_arguments(args,integer_count,xmm_count);
    if(!target) throw std::runtime_error("Original register target missing");
    OriginalRegisterResult result;result.integer=ctr_call_original_registers(target,args.integers.data(),args.xmm_bits.data(),&result.xmm_bits);return result;
}
extern "C" void ctr_sysv_register_fixture();
extern "C" int ctr_original_register_preservation_control();
inline int original_register_control() {
    if(ctr_original_register_preservation_control()!=1) throw std::runtime_error("Win64 nonvolatile register preservation differs");
    std::array<std::uint64_t,14> observed{};OriginalRegisterArguments args;
    args.integer_count=6;args.xmm_count=8;
    args.integers={reinterpret_cast<std::uint64_t>(observed.data()),11,22,33,44,55};
    for(unsigned i=0;i<8;++i) {args.xmm_bits[i]=0x3ff0000000000000ULL+i;args.xmm_kinds[i]=XmmArgumentKind::Float64;}
    auto result=invoke_original_registers(reinterpret_cast<void*>(&ctr_sysv_register_fixture),args,6,8);
    for(unsigned i=0;i<6;++i) if(observed[i]!=args.integers[i]) throw std::runtime_error("Integer register routing differs");
    for(unsigned i=0;i<8;++i) if(observed[i+6]!=args.xmm_bits[i]) throw std::runtime_error("XMM register routing differs");
    if(result.integer!=55 || result.xmm_bits!=args.xmm_bits[7]) throw std::runtime_error("Original result routing differs");
    args.return_kind=OriginalReturnKind::Float32;
    for(unsigned i=0;i<8;++i) {args.xmm_bits[i]=0x3f800000U+i;args.xmm_kinds[i]=XmmArgumentKind::Float32;}
    result=invoke_original_registers(reinterpret_cast<void*>(&ctr_sysv_register_fixture),args,6,8);
    if(result.xmm_bits!=args.xmm_bits[7]) throw std::runtime_error("Float result routing differs");
    unsigned rejected=0;
    try {validate_register_arguments(args,5,8);} catch(const std::runtime_error&) {++rejected;}
    args.xmm_kinds[0]=static_cast<XmmArgumentKind>(99);
    try {validate_register_arguments(args,6,8);} catch(const std::runtime_error&) {++rejected;}
    args.xmm_kinds[0]=XmmArgumentKind::Float32;args.xmm_bits[0]=0x100000000ULL;
    try {validate_register_arguments(args,6,8);} catch(const std::runtime_error&) {++rejected;}
    if(rejected!=3) throw std::runtime_error("Register negatives differ");
    std::cout<<"{\"scope\":\"synthetic_register_call\",\"integer_registers\":6,\"xmm_registers\":8,\"float_double_and_integer_returns\":true,\"negative_rejections\":3,\"rdi_rsi_xmm6_through15_preserved\":true}\n";
    return 0;
}

}
