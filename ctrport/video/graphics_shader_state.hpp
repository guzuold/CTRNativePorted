// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "pm4_registers.hpp"
namespace CtrPm4 {
// Exact public Regs::ShaderProgram: PS at2c08, VS at2c48;20DWORD each.
// Value metadata only, no graphics pipeline/layout or descriptors inferred.
struct GraphicsShaderState {
    uint64_t codeAddress = 0;
    uint32_t userRegisters = 0, allocatedVgprs = 0, inputVgprs = 0;
    uint32_t fpRound32 = 0, fpRound64 = 0, fpDenorm32 = 0, fpDenorm64 = 0;
    bool scratch = false, ocLds = false;
    std::array<uint32_t, 16> userData{};
};
inline int CaptureGraphicsShaderState(const Registers& registers, bool vertex, GraphicsShaderState& output) {
    const size_t base = vertex ? 0x2c48 : 0x2c08;
    const auto& r = registers.words;
    GraphicsShaderState prepared;
    prepared.codeAddress = (uint64_t(r[base]) | (uint64_t(r[base + 1] & 255) << 32)) << 8;
    if (!prepared.codeAddress) return 88;
    const uint32_t first = r[base + 2], second = r[base + 3];
    prepared.userRegisters = (second >> 1) & 31;
    if (prepared.userRegisters > 16) return 88;
    prepared.allocatedVgprs = ((first & 63) + 1) * 4;
    prepared.inputVgprs = (first >> 24) & 3;
    prepared.fpRound32 = (first >> 12) & 3;
    prepared.fpRound64 = (first >> 14) & 3;
    prepared.fpDenorm32 = (first >> 16) & 3;
    prepared.fpDenorm64 = (first >> 18) & 3;
    prepared.scratch = (second & 1) != 0;
    prepared.ocLds = ((second >> 7) & 1) != 0;
    for (size_t i = 0; i < 16; ++i) prepared.userData[i] = r[base + 4 + i];
    output = prepared;
    return 0;
}
}
