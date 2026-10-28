// SPDX-License-Identifier: GPL-2.0-only
// x86-64 instruction length decoder for the game's code: the length of one instruction and
// where its displacement and immediate fields lie, enough to tell instruction boundaries and
// to move RIP-relative references and relative branches. Covers the legacy, 0F, 0F38 and 0F3A
// maps, VEX and AMD SSE4a (the PS4 CPU has AVX, no AVX-512). Written from the Intel SDM and
// AMD APM opcode maps and checked against capstone on every function of the EU and US eboots
// (tools/region_map.py --check-decoder).
#pragma once
#include <cstddef>
#include <cstdint>
namespace ctrport::x86 {

struct Insn {
    int length = 0;                     // 0: not decodable
    int disp_offset = 0, disp_size = 0; // ModRM/SIB displacement or moffs
    int imm_offset = 0, imm_size = 0;   // first immediate (relative branch target included)
    bool rip = false;                   // [rip + disp32]
    bool relative = false;              // imm is a branch displacement (call, jmp, jcc, loop)
};

// Decodes the instruction at code[0..available).
inline Insn decode(const unsigned char* code, std::size_t available) {
    Insn out;
    std::size_t i = 0;
    bool opsize16 = false, addr32 = false, rex_w = false, repne = false;
    const auto byte = [&](std::size_t at) -> int { return at < available ? code[at] : -1; };
    // legacy prefixes, then REX
    for (;;) {
        const int b = byte(i);
        if (b == 0x66)
            opsize16 = true;
        else if (b == 0x67)
            addr32 = true;
        else if (b == 0xF2)
            repne = true;
        else if (b == 0xF0 || b == 0xF3 || b == 0x2E || b == 0x36 || b == 0x3E || b == 0x26 || b == 0x64 || b == 0x65) {
        } else
            break;
        if (++i > 14) return out;
    }
    if ((byte(i) & 0xF0) == 0x40) {
        rex_w = (byte(i) & 8) != 0;
        ++i;
    }
    int op = byte(i++);
    if (op < 0) return out;
    int map = 0; // 0: one byte, 1: 0F, 2: 0F38, 3: 0F3A
    bool vex = false;
    if (op == 0xC4 || op == 0xC5) { // VEX (C4/C5 are LES/LDS only outside 64-bit mode)
        vex = true;
        if (op == 0xC5) {
            if (byte(i) < 0) return out;
            map = 1;
            i += 1;
        } else {
            const int b1 = byte(i), b2 = byte(i + 1);
            if (b1 < 0 || b2 < 0) return out;
            map = b1 & 0x1F;
            if (map < 1 || map > 3) return out;
            rex_w = (b2 & 0x80) != 0;
            i += 2;
        }
        op = byte(i++);
        if (op < 0) return out;
    } else if (op == 0x0F) {
        map = 1;
        op = byte(i++);
        if (op == 0x38 || op == 0x3A) {
            map = op == 0x38 ? 2 : 3;
            op = byte(i++);
        }
        if (op < 0) return out;
    }
    bool modrm = false;
    int imm = 0;        // immediate bytes
    bool imm_z = false; // 16 or 32 bits by operand size
    bool relative = false;
    if (map == 0) {
        if (op == 0x62 || op == 0x06 || op == 0x07 || op == 0x0E || op == 0x16 || op == 0x17 || op == 0x1E ||
            op == 0x1F || op == 0x27 || op == 0x2F || op == 0x37 || op == 0x3F || op == 0x60 || op == 0x61 ||
            op == 0x82 || op == 0x9A || op == 0xCE || op == 0xD4 || op == 0xD5 || op == 0xD6 || op == 0xEA)
            return out; // invalid in 64-bit mode
        if (op < 0x40) {
            const int low = op & 7;
            if (low < 4)
                modrm = true;
            else if (low == 4)
                imm = 1;
            else if (low == 5)
                imm_z = true;
        } else if (op < 0x60) {
        } else if (op == 0x63)
            modrm = true;
        else if (op == 0x68)
            imm_z = true;
        else if (op == 0x69)
            modrm = true, imm_z = true;
        else if (op == 0x6A)
            imm = 1;
        else if (op == 0x6B)
            modrm = true, imm = 1;
        else if (op < 0x70) {
        } else if (op < 0x80)
            imm = 1, relative = true;
        else if (op == 0x80 || op == 0x83 || op == 0xC0 || op == 0xC1 || op == 0xC6)
            modrm = true, imm = 1;
        else if (op == 0x81 || op == 0xC7)
            modrm = true, imm_z = true;
        else if (op < 0x90)
            modrm = true;
        else if (op < 0xA0) {
        } else if (op < 0xA4) { // mov al/eax, moffs
            out.disp_offset = static_cast<int>(i);
            out.disp_size = addr32 ? 4 : 8;
            i += out.disp_size;
        } else if (op == 0xA8)
            imm = 1;
        else if (op == 0xA9)
            imm_z = true;
        else if (op < 0xB0) {
        } else if (op < 0xB8)
            imm = 1;
        else if (op < 0xC0)
            imm = rex_w ? 8 : (opsize16 ? 2 : 4);
        else if (op == 0xC2 || op == 0xCA)
            imm = 2;
        else if (op == 0xC8)
            imm = 3;
        else if (op == 0xCD)
            imm = 1;
        else if (op < 0xD0) {
        } else if (op < 0xD4)
            modrm = true;
        else if (op == 0xD7) {
        } else if (op < 0xE0)
            modrm = true;
        else if (op < 0xE4)
            imm = 1, relative = true;
        else if (op < 0xE8)
            imm = 1;
        else if (op == 0xE8 || op == 0xE9)
            imm = 4, relative = true; // rel32 whatever the operand size
        else if (op == 0xEB)
            imm = 1, relative = true;
        else if (op < 0xF6) {
        } else if (op == 0xF6 || op == 0xF7) {
            modrm = true;
            const int m = byte(i);
            if (m < 0) return out;
            if (((m >> 3) & 7) < 2) {
                if (op == 0xF6)
                    imm = 1;
                else
                    imm_z = true;
            }
        } else if (op >= 0xFE)
            modrm = true;
    } else if (map == 1) {
        if (vex && op == 0x77) {
        } else if (op == 0x0F) {
            modrm = true, imm = 1; // 3DNow!
        } else if (op < 0x04 || op == 0x0D || (op >= 0x10 && op < 0x30 && !(op >= 0x24 && op < 0x28))) {
            modrm = true;
        } else if (op < 0x40) {
            if (op >= 0x24 && op < 0x28) return out;
        } else if (op < 0x70)
            modrm = true;
        else if (op < 0x74)
            modrm = true, imm = 1;
        else if (op == 0x78 && !vex && (repne || opsize16))
            modrm = true, imm = 2; // SSE4a insertq/extrq imm8, imm8
        else if (op < 0x80) {
            if (op != 0x77) modrm = true;
        } else if (op < 0x90)
            imm = 4, relative = true; // jcc rel32 (66 is ignored in 64-bit mode)
        else if (op < 0xA0)
            modrm = true;
        else if (op == 0xA4 || op == 0xAC || op == 0xBA || op == 0xC2 || (op >= 0xC4 && op <= 0xC6))
            modrm = true, imm = 1;
        else if (op == 0xA3 || op == 0xA5 || (op >= 0xAB && op < 0xC8))
            modrm = true;
        else if (op < 0xC8) {
        } else if (op < 0xD0) {
        } else
            modrm = true;
    } else if (map == 2) {
        modrm = true;
    } else {
        modrm = true, imm = 1;
    }
    if (imm_z) imm = opsize16 ? 2 : 4;
    if (modrm) {
        const int m = byte(i++);
        if (m < 0) return out;
        const int mod = m >> 6, rm = m & 7;
        if (mod != 3) {
            int disp = mod == 1 ? 1 : mod == 2 ? 4 : 0;
            if (rm == 4) {
                const int sib = byte(i++);
                if (sib < 0) return out;
                if (mod == 0 && (sib & 7) == 5) disp = 4;
            } else if (mod == 0 && rm == 5) {
                disp = 4;
                out.rip = true;
            }
            if (disp) {
                out.disp_offset = static_cast<int>(i);
                out.disp_size = disp;
                i += disp;
            }
        }
    }
    if (imm) {
        out.imm_offset = static_cast<int>(i);
        out.imm_size = imm;
        out.relative = relative;
        i += imm;
    }
    if (i > available || i > 15) return out;
    out.length = static_cast<int>(i);
    return out;
}

} // namespace ctrport::x86
