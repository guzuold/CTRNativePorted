// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "original_call.hpp"
#include <array>
namespace ctrport {
enum class XmmArgumentKind { Float32, Float64 };
enum class OriginalReturnKind { Integer, Float32, Float64 };
struct OriginalRegisterArguments {
    std::array<std::uint64_t, 6> integers{};
    std::array<std::uint64_t, 8> xmm_bits{};
    std::array<XmmArgumentKind, 8> xmm_kinds{};
    unsigned integer_count = 0, xmm_count = 0;
    OriginalReturnKind return_kind = OriginalReturnKind::Integer;
};
struct OriginalRegisterResult {
    std::uint64_t integer = 0, xmm_bits = 0;
};
extern "C" std::uint64_t ctr_call_original_registers(void*, const std::uint64_t*, const std::uint64_t*, std::uint64_t*);
inline void validate_register_arguments(const OriginalRegisterArguments& args, unsigned declared_integer_count,
                                        unsigned declared_xmm_count) {
    if (args.integer_count > 6 || args.xmm_count > 8 || args.integer_count != declared_integer_count ||
        args.xmm_count != declared_xmm_count)
        throw std::runtime_error("Original register count differs from declaration");
    for (unsigned i = 0; i < args.xmm_count; ++i) {
        if (args.xmm_kinds[i] != XmmArgumentKind::Float32 && args.xmm_kinds[i] != XmmArgumentKind::Float64)
            throw std::runtime_error("Original XMM type invalid");
        if (args.xmm_kinds[i] == XmmArgumentKind::Float32 && args.xmm_bits[i] >> 32)
            throw std::runtime_error("Original float32 payload exceeds32bits");
    }
    if (args.return_kind != OriginalReturnKind::Integer && args.return_kind != OriginalReturnKind::Float32 &&
        args.return_kind != OriginalReturnKind::Float64)
        throw std::runtime_error("Original return type invalid");
}
inline OriginalRegisterResult invoke_original_registers(void* target, const OriginalRegisterArguments& args,
                                                        unsigned integer_count, unsigned xmm_count) {
    validate_register_arguments(args, integer_count, xmm_count);
    if (!target) throw std::runtime_error("Original register target missing");
    OriginalRegisterResult result;
    result.integer = ctr_call_original_registers(target, args.integers.data(), args.xmm_bits.data(), &result.xmm_bits);
    return result;
}
extern "C" void ctr_sysv_register_fixture();
extern "C" int ctr_original_register_preservation_control();

}
