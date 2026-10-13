// CTRPort adaptation: descriptor capture traversal unavailable; reject SRT roots/serialized walkers explicitly. No JIT or fault-to-zero recovery.
// Origin: engine/shadPS4/src/shader_recompiler/ir/passes/flatten_extended_userdata_pass.cpp @ e3ce810f3a653f43ac64ebab63023de281a4103a
// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <unordered_map>
#include <stdexcept>
#include "srt_missing_diagnostic.hpp"
#include "owned_srt_leaf.hpp"
#include <boost/container/flat_map.hpp>
#include "common/arch.h"
#include "common/logging/log.h"

#include "shader_recompiler/info.h"
#include "shader_recompiler/ir/breadth_first_search.h"
#include "shader_recompiler/ir/opcodes.h"
#include "shader_recompiler/ir/passes/srt.h"
#include "shader_recompiler/ir/program.h"
#include "shader_recompiler/ir/reg.h"
#include "shader_recompiler/ir/srt_gvn_table.h"
#include "shader_recompiler/ir/value.h"

#ifdef ARCH_X86_64

namespace Shader {

PFN_SrtWalker RegisterWalkerCode(const u8* ptr, size_t size) {
    (void)ptr; (void)size;
    throw std::runtime_error("Serialized SRT walker execution missing: checked descriptor capture required");
}

} // namespace Shader

namespace {

using namespace Shader;

struct PassInfo {
    // map offset to inst
    using PtrUserList = boost::container::flat_map<u32, Shader::IR::Inst*>;

    Optimization::SrtGvnTable gvn_table;
    // keys are GetUserData or ReadConst instructions that are used as pointers
    std::unordered_map<IR::Inst*, PtrUserList> pointer_uses;
    // GetUserData instructions corresponding to sgpr_base of SRT roots
    boost::container::small_flat_map<IR::ScalarReg, IR::Inst*, 1> srt_roots;

    // pick a single inst for a given value number
    std::unordered_map<u32, IR::Inst*> vn_to_inst;

    // Bumped during codegen to assign offsets to readconsts
    u32 dst_off_dw;

    PtrUserList* GetUsesAsPointer(IR::Inst* inst) {
        auto it = pointer_uses.find(inst);
        if (it != pointer_uses.end()) {
            return &it->second;
        }
        return nullptr;
    }

    // Return a single instruction that this instruction is identical to, according
    // to value number
    // The "original" is arbitrary. Here it's the first instruction found for a given value number
    IR::Inst* DeduplicateInstruction(IR::Inst* inst) {
        auto it = vn_to_inst.try_emplace(gvn_table.GetValueNumber(inst), inst);
        return it.first->second;
    }
};
} // namespace

namespace Shader::Optimization {

namespace {

static void GenerateSrtProgram(Info& info, PassInfo& pass_info) {
    if (!pass_info.srt_roots.empty()) {
        auto* context=CtrPm4::activeFetchContext;
        if(context && context->allowSrtLeaf) {
            std::vector<CtrPm4::SrtLeafRoot> roots;
            for(const auto& [reg,inst]:pass_info.srt_roots) {
                auto* uses=pass_info.GetUsesAsPointer(inst);
                if(!uses)throw std::runtime_error("SRT root uses missing");
                CtrPm4::SrtLeafRoot root{static_cast<u32>(reg),{}};
                for(const auto& [offset,use]:*uses) {
                    if(pass_info.GetUsesAsPointer(use))
                        throw std::runtime_error("Nested SRT pointer traversal missing");
                    root.offsets.push_back(offset);
                }
                roots.push_back(std::move(root));
            }
            if(pass_info.pointer_uses.size()!=roots.size())
                throw std::runtime_error("Nonleaf SRT pointer graph missing");
            std::vector<CtrPm4::SrtProvenanceRoot> provenanceRoots;
            for(const auto& root:roots)provenanceRoots.push_back({root.sgpr,root.offsets});
            std::vector<CtrPm4::SrtProvenanceWord> provenance;
            if(CtrPm4::BuildSrtLeafProvenance(context->declaredWords,provenanceRoots,provenance))
                throw std::runtime_error("SRT leaf provenance shape rejected");
            std::vector<u32> flattened;
            if(CtrPm4::CaptureSrtLeafRoots(*context,info.user_data,roots,flattened))
                throw std::runtime_error("Owned SRT leaf capture missing");
            u32 destination=16;
            for(const auto& [reg,inst]:pass_info.srt_roots) {
                (void)reg;
                for(const auto& [offset,use]:*pass_info.GetUsesAsPointer(inst)) {
                    (void)offset;use->SetFlags<u32>(destination++);
                }
            }
            if(provenance.size()+16!=destination || flattened.size()!=destination)
                throw std::runtime_error("SRT leaf provenance extent mismatch");
            context->srtProvenance=std::move(provenance);
            info.srt_info.flattened_bufsize_dw=destination;
            info.flattened_ud_buf=std::move(flattened);
            return;
        }
        CtrPm4::SrtMissingDiagnostic diagnostic;
        diagnostic.pointer_nodes=static_cast<u32>(pass_info.pointer_uses.size());
        for(const auto& [reg,inst]:pass_info.srt_roots) {
            CtrPm4::SrtMissingDiagnostic::Root root{static_cast<u32>(reg),{}};
            if(auto* uses=pass_info.GetUsesAsPointer(inst))
                for(const auto& [offset,user]:*uses){(void)user;root.offsets.push_back(offset);}
            diagnostic.roots.push_back(std::move(root));
        }
        throw diagnostic;
    }
}

}; // namespace

void FlattenExtendedUserdataPass(IR::Program& program) {
    Shader::Info& info = program.info;
    PassInfo pass_info;

    // traverse at end and assign offsets to duplicate readconsts, using
    // vn_to_inst as the source
    boost::container::small_vector<IR::Inst*, 32> all_readconsts;

    for (auto r_it = program.post_order_blocks.rbegin(); r_it != program.post_order_blocks.rend();
         r_it++) {
        IR::Block* block = *r_it;
        for (IR::Inst& inst : *block) {
            if (inst.GetOpcode() == IR::Opcode::ReadConst) {
                if (!inst.Arg(1).IsImmediate()) {
                    LOG_WARNING(Render_Recompiler, "ReadConst has non-immediate offset");
                    continue;
                }

                all_readconsts.push_back(&inst);
                if (pass_info.DeduplicateInstruction(&inst) != &inst) {
                    // This is a duplicate of a readconst we've already visited
                    continue;
                }

                IR::Inst* ptr_composite = inst.Arg(0).InstRecursive();

                const auto pred = [](IR::Inst* inst) -> std::optional<IR::Inst*> {
                    if (inst->GetOpcode() == IR::Opcode::GetUserData ||
                        inst->GetOpcode() == IR::Opcode::ReadConst) {
                        return inst;
                    }
                    return std::nullopt;
                };
                auto base0 = IR::BreadthFirstSearch(ptr_composite->Arg(0), pred);
                auto base1 = IR::BreadthFirstSearch(ptr_composite->Arg(1), pred);
                ASSERT_MSG(base0 && base1, "ReadConst not from constant memory");

                IR::Inst* ptr_lo = base0.value();
                ptr_lo = pass_info.DeduplicateInstruction(ptr_lo);

                auto ptr_uses_kv =
                    pass_info.pointer_uses.try_emplace(ptr_lo, PassInfo::PtrUserList{});
                PassInfo::PtrUserList& user_list = ptr_uses_kv.first->second;

                user_list[inst.Arg(1).U32()] = &inst;

                if (ptr_lo->GetOpcode() == IR::Opcode::GetUserData) {
                    IR::ScalarReg ud_reg = ptr_lo->Arg(0).ScalarReg();
                    pass_info.srt_roots[ud_reg] = ptr_lo;
                }
            }
        }
    }

    GenerateSrtProgram(info, pass_info);

    // Assign offsets to duplicate readconsts
    for (IR::Inst* readconst : all_readconsts) {
        ASSERT(pass_info.vn_to_inst.contains(pass_info.gvn_table.GetValueNumber(readconst)));
        IR::Inst* original = pass_info.DeduplicateInstruction(readconst);
        readconst->SetFlags<u32>(original->Flags<u32>());
    }

    info.RefreshFlatBuf();
}

} // namespace Shader::Optimization

#else

namespace Shader {

PFN_SrtWalker RegisterWalkerCode(const u8* ptr, size_t size) {
    UNREACHABLE_MSG("RegisterWalkerCode unimplemented for target architecture.");
}

namespace Optimization {

void FlattenExtendedUserdataPass(IR::Program& program) {
    UNREACHABLE_MSG("FlattenExtendedUserdataPass unimplemented for target architecture.");
}

} // namespace Optimization

} // namespace Shader

#endif
