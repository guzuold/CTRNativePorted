// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video/pm4_init_plan.hpp"
namespace ctrport {
// Diagnostic register observation only. Whole public SetConfigReg and IndexType
// semantics from shadPS4 liverpool.cpp/regs.cpp at
// e3ce810f3a653f43ac64ebab63023de281a4103a; no queue or draw execution.
inline int observe_pm4_register(const CtrPm4::InitOperation& op, CtrPm4::Registers& registers) {
    const auto& p = op.payload;
    if (op.opcode == 0x68) {
        if (p.size() < 2 || p[0] > 0xffff || p[0] >= 0xb000 || p.size() - 1 > 0xb000 - p[0]) return 60;
        std::memcpy(registers.words.data() + 0x2000 + p[0], p.data() + 1, (p.size() - 1) * sizeof(uint32_t));
        return 0;
    }
    if (op.opcode == 0x2a) {
        if (p.size() != 1) return 60;
        registers.words[0xa29f] = p[0];
        return 0;
    }
    return CtrPm4::ApplyRegister(op, registers);
}
}
