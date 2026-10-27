// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "pm4_registers.hpp"
#include <algorithm>
// Value-only compiler input metadata from pinned public ComputeProgram and
// PipelineCache::BuildRuntimeInfo, e3ce810f3a653f43ac64ebab63023de281a4103a.
// No guest dereference, shader size discovery, descriptors or dispatch execution.
namespace CtrPm4 {
struct ComputeShaderState {
    uint64_t codeAddress = 0;
    std::array<uint32_t, 3> workgroup{}, partial{};
    uint32_t userRegisters = 0, allocatedVgprs = 0, sharedBytes = 0;
    std::array<bool, 3> tgid{};
    std::array<uint32_t, 16> userData{};
};
inline int CaptureComputeShaderState(const Registers& registers, ComputeShaderState& output) {
    const auto& c = registers.compute;
    ComputeShaderState prepared;
    prepared.codeAddress = (uint64_t(c[12]) | (uint64_t(c[13] & 255) << 32)) << 8;
    if (!prepared.codeAddress) return 88;
    for (size_t i = 0; i < 3; ++i) {
        prepared.workgroup[i] = c[7 + i] & 65535;
        prepared.partial[i] = c[7 + i] >> 16;
        prepared.tgid[i] = ((c[19] >> (7 + i)) & 1) != 0;
    }
    prepared.userRegisters = (c[19] >> 1) & 31;
    if (prepared.userRegisters > 16) return 88; // Current snapshot owns16DWORD only.
    prepared.allocatedVgprs = (c[18] & 63) * 4; // Exact BuildRuntimeInfo convention.
    prepared.sharedBytes = ((c[19] >> 15) & 511) * 512;
    std::copy(c.begin() + 64, c.end(), prepared.userData.begin());
    output = prepared;
    return 0;
}
}
