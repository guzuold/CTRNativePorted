// SPDX-License-Identifier: GPL-2.0-only
// Shader variants of the PC settings that change GCN code (ambient occlusion off, hero shadows off),
// made with the same recognised-shader contracts the GPU library applies before its translation
// (ctrport/gpu/ao_controls, ctrport/gpu/shadow_quality). Its own translation unit: those headers
// are C++20, the archive reader ctr_shaders shares with the launcher is C++17.
#include "variants.h"

#include <cstring>
#include <stdexcept>

#include "../../gpu/shadow_quality/hero_shader.hpp"

namespace {
std::vector<uint8_t> patched(const std::vector<uint8_t>& code, const std::vector<uint32_t>& words) {
    std::vector<uint8_t> out(code);
    std::memcpy(out.data(), words.data(), words.size() * 4);
    return out;
}
std::span<const uint32_t> prefix(const std::vector<uint8_t>& code, size_t words, std::vector<uint32_t>& storage) {
    storage.resize(words);
    std::memcpy(storage.data(), code.data(), words * 4);
    return storage;
}
} // namespace

std::vector<ShaderVariant> shader_variants(const std::vector<uint8_t>& code) {
    std::vector<ShaderVariant> out;
    std::vector<uint32_t> words;
    for (const auto& d : ctrport::settings::ao::descriptors) {
        if (code.size() < d.words * 4) continue;
        try {
            auto w = ctrport::settings::ao::override_code(d.hash, prefix(code, d.words, words), true);
            if (!w.empty()) out.push_back({"ao_off", patched(code, w)});
        } catch (const std::exception&) { // another shader of this size
        }
    }
    namespace hero = ctrport::settings::hero_shadows;
    if (code.size() >= 161 * 4) {
        try {
            auto w = hero::override_code(hero::shader_hash, prefix(code, 161, words), true);
            if (!w.empty()) out.push_back({"hero_off", patched(code, w)});
        } catch (const std::exception&) {}
    }
    return out;
}
