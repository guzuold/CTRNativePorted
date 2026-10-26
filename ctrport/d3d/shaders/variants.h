// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
#include <string>
#include <vector>

// A PC setting's version of a shader: the setting name as the runtime selects it (ao_off,
// hero_off) and the patched GCN binary, translated like the original.
struct ShaderVariant {
    std::string setting;
    std::vector<uint8_t> code;
};
std::vector<ShaderVariant> shader_variants(const std::vector<uint8_t>& code);
