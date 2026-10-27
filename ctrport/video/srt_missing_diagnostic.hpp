// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdint>
namespace CtrPm4 {
struct SrtMissingDiagnostic : std::runtime_error {
    struct Root {
        uint32_t sgpr;
        std::vector<uint32_t> offsets;
    };
    std::vector<Root> roots;
    uint32_t pointer_nodes = 0;
    SrtMissingDiagnostic()
        : std::runtime_error("SRT descriptor capture traversal missing: shader has extended userdata roots") {}
    std::string Describe() const {
        std::string text = what();
        text += "; roots=" + std::to_string(roots.size());
        text += " pointer_nodes=" + std::to_string(pointer_nodes);
        for (const auto& root : roots) {
            text += " sgpr=" + std::to_string(root.sgpr) + " offsets_dw=";
            for (auto offset : root.offsets) text += std::to_string(offset) + ",";
        }
        return text;
    }
};
}
