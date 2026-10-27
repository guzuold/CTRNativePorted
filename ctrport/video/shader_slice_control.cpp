// SPDX-License-Identifier: GPL-2.0-only
#include "adapted/include/shader_recompiler/frontend/decode.h"
#include <limits>
#include <stdexcept>
using Shader::Gcn::GcnCodeSlice;
template <class F> void Reject(F action) {
    try {
        action();
    } catch (const std::out_of_range&) {
        return;
    }
    throw std::runtime_error("Truncated input accepted");
}
int main() {
    const u32 words[] = {0x12345678, 0x89abcdef, 0x10203040};
    GcnCodeSlice empty(nullptr, nullptr);
    if (!empty.atEnd()) return 1;
    Reject([&] { (void)empty.at(0); });
    Reject([&] { (void)empty.readu32(); });
    Reject([&] { (void)empty.readu64(); });
    GcnCodeSlice one(words, words + 1);
    Reject([&] { (void)one.readu64(); });
    Reject([&] { (void)one.at(1); });
    Reject([&] { (void)one.at((std::numeric_limits<u32>::max)()); });
    if (one.at(0) != words[0] || one.readu32() != words[0] || !one.atEnd()) return 2;
    Reject([&] { (void)one.readu32(); });
    GcnCodeSlice whole(words, words + 3);
    if (whole.readu32() != words[0] || whole.readu64() != 0x1020304089abcdefULL || !whole.atEnd()) return 3;
    Reject([&] { (void)whole.at(0); });
    GcnCodeSlice copied(words, words + 3);
    auto second = copied;
    if (second.readu64() != 0x89abcdef12345678ULL || copied.readu32() != words[0]) return 4;
    using namespace Shader::Gcn;
    GcnDecodeContext decoder;
    GcnCodeSlice no_instruction(nullptr, nullptr);
    Reject([&] { (void)decoder.decodeInstruction(no_instruction); });
    const u32 literal[] = {0xBEEB03FF, 0x12345678}; // s_mov_b32 vcc_hi, literal
    GcnCodeSlice missing_literal(literal, literal + 1);
    Reject([&] { (void)decoder.decodeInstruction(missing_literal); });
    GcnCodeSlice complete_literal(literal, literal + 2);
    auto mov = decoder.decodeInstruction(complete_literal);
    if (mov.opcode != Opcode::S_MOV_B32 || mov.length != 8 || !complete_literal.atEnd()) return 5;
    const u32 wide[] = {0xF8000000, 0}; // EXP encoding, two dwords
    GcnCodeSlice short_wide(wide, wide + 1);
    Reject([&] { (void)decoder.decodeInstruction(short_wide); });
    GcnCodeSlice complete_wide(wide, wide + 2);
    auto exp = decoder.decodeInstruction(complete_wide);
    if (exp.length != 8 || !complete_wide.atEnd()) return 6;
    const u32 end[] = {0xBF810000};
    GcnCodeSlice complete_end(end, end + 1);
    auto stop = decoder.decodeInstruction(complete_end);
    if (stop.opcode != Opcode::S_ENDPGM || stop.length != 4 || !complete_end.atEnd()) return 7;
    return 0;
}
