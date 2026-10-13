// CTRPort adaptation: shader dump uses explicit owned session/streams; no mkdir or overwrite.
// Origin: engine/shadPS4/src/shader_recompiler/ir/program.cpp @ e3ce810f3a653f43ac64ebab63023de281a4103a
// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <map>
#include <string>

#include <fmt/format.h>

#include "shader_directory_pin.h"

#include "core/emulator_settings.h"
#include "shader_recompiler/ir/basic_block.h"
#include "shader_recompiler/ir/program.h"
#include "shader_recompiler/ir/value.h"

namespace Shader::IR {

void DumpProgram(const Program& program, const Info& info, const std::string& type) {


    if (!EmulatorSettings.IsDumpShaders()) {
        return;
    }

    const auto ir_filename =
        fmt::format("{}_{:#018x}.{}irprogram.txt", info.stage, info.pgm_hash, type);
    const auto ir_file = CtrVideo::GetShaderDumpSession().Open(ir_filename);

    size_t index{0};
    std::map<const IR::Inst*, size_t> inst_to_index;
    std::map<const IR::Block*, size_t> block_to_index;

    for (const IR::Block* const block : program.blocks) {
        block_to_index.emplace(block, index);
        ++index;
    }

    for (const auto& block : program.blocks) {
        std::string s = IR::DumpBlock(*block, block_to_index, inst_to_index, index) + '\n';
        ir_file->Write(s.data(),s.size());
    }

    ir_file->Finish();
    const auto asl_filename = fmt::format("{}_{:#018x}.{}asl.txt", info.stage, info.pgm_hash, type);
    const auto asl_file =
        CtrVideo::GetShaderDumpSession().Open(asl_filename);

    for (const auto& node : program.syntax_list) {
        std::string s = IR::DumpASLNode(node, block_to_index, inst_to_index) + '\n';
        asl_file->Write(s.data(),s.size());
    }
    asl_file->Finish();
}

} // namespace Shader::IR
