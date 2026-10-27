// SPDX-License-Identifier: GPL-2.0-only
#include "hero_shader.hpp"
#include "hero_patch.hpp"
#include <fstream>
#include <iostream>
#include <cmath>
#include <limits>
#include <immintrin.h>
using namespace ctrport::settings::hero_shadows;
extern "C" void probe_hero(void*, void*, void*);
static unsigned checks = 0, calls = 0;
void check(bool value) {
    ++checks;
    if (!value) throw std::runtime_error("hero shadow assertion failed");
}
template <class F> void rejects(F fn) {
    bool caught = false;
    try {
        fn();
    } catch (const std::exception&) {
        caught = true;
    }
    check(caught);
}
template <class T> void put(unsigned char* p, std::size_t at, T v) {
    std::memcpy(p + at, &v, sizeof(v));
}
template <class T> T get(const unsigned char* p, std::size_t at) {
    T v;
    std::memcpy(&v, p + at, sizeof(v));
    return v;
}
constexpr auto identity = "39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5";
struct State {
    std::array<unsigned char, 0x100> object{}, context{}, cache{};
    std::array<unsigned char, 0x700> renderer{};
    std::array<unsigned char, 0x100> owner{};
    unsigned value = 0;
    void reset(unsigned enabled, unsigned previous, bool dirty) {
        object.fill(0);
        context.fill(0);
        cache.fill(0);
        renderer.fill(0);
        owner.fill(0);
        value = previous;
        object[0x18] = static_cast<unsigned char>(enabled);
        put(context.data(), 0x40, renderer.data());
        put(renderer.data(), 0x5a8, owner.data());
        put(cache.data(), 0, &value);
        put(cache.data(), 8, 0x101u);
        put(owner.data(), 0x6c, 19u);
        if (dirty) put(cache.data(), 0x38, owner.data());
    }
    std::array<unsigned, 6> outcome() {
        return {value,
                get<unsigned>(owner.data(), 0x6c),
                get<unsigned>(cache.data(), 0x40),
                get<unsigned>(cache.data(), 8),
                get<void*>(cache.data(), 0x38) != nullptr,
                get<void*>(owner.data(), 0x70) != nullptr};
    }
};
// Decode actual original V_MAD source operands and compare with neutral fade0.
float operand(unsigned field, float visibility, float fade) {
    if (field == 8) return fade;
    if (field == 256) return visibility;
    if (field == 242) return 1.f;
    throw std::runtime_error("unsupported hero operand");
}
std::uint32_t suffix(const std::vector<std::uint32_t>& code, float visibility, float fade) {
    float result{};
    if (code[154] == 0xd2820000) {
        const auto x = code[155];
        check((x >> 27) == 0);
        result = operand(x & 511, visibility, fade) * operand((x >> 9) & 511, visibility, fade) +
                 operand((x >> 18) & 511, visibility, fade);
    } else {
        check((code[154] >> 25) == 0x3f && ((code[154] >> 9) & 255) == 1 && (code[154] & 511) == 242 &&
              ((code[154] >> 17) & 255) == 0 && code[155] == 0xbf800000);
        result = 1.f;
    }
    check(code[156] == 0x5e000100 && code[157] == 0xbefe040e && code[158] == 0xf8001c0f && code[159] == 0 &&
          code[160] == 0xbf810000);
    auto packed = _mm_cvtps_ph(_mm_set1_ps(result), 3);
    return static_cast<std::uint32_t>(_mm_cvtsi128_si32(packed));
}
int main(int argc, char** argv) {
    try {
        check(argc == 3);
        std::ifstream sf(argv[1], std::ios::binary);
        std::vector<std::uint32_t> shader(161);
        sf.read(reinterpret_cast<char*>(shader.data()), 644);
        check(bool(sf));
        const auto saved = shader;
        auto changed = override_code(shader_hash, shader, true);
        check(shader == saved && changed.size() == shader.size());
        check(override_code(shader_hash, shader, false).empty() && override_code(0xdeadbeef, shader, true).empty());
        rejects([&] { override_code(shader_hash, changed, true); });
        for (std::size_t i = 0; i < shader.size(); ++i) {
            auto bad = shader;
            bad[i] ^= 1;
            rejects([&] { override_code(shader_hash, bad, true); });
        }
        for (float visibility : {-1.f, -0.5f, 0.f, 0.5f, 1.f})
            for (float fade : {0.f, 0.25f, 1.f}) {
                check(suffix(changed, visibility, fade) == suffix(shader, visibility, 0.f));
                check(suffix(changed, visibility, fade) == 0x3c003c00);
            }
        check(suffix(shader, -0.5f, 1.f) != suffix(changed, -0.5f, 1.f));
        for (float exceptional : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                                  -std::numeric_limits<float>::infinity()})
            check(suffix(changed, exceptional, exceptional) == 0x3c003c00);
        constexpr std::size_t extent = 0x2b00000;
        auto* image = static_cast<unsigned char*>(
            VirtualAlloc(nullptr, extent, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        check(image != nullptr);
        std::ifstream pf(argv[2], std::ios::binary);
        pf.read(reinterpret_cast<char*>(image + 0x9504a0), 1589);
        check(bool(pf));
        const std::vector<unsigned char> original_code(image + 0x9504a0, image + 0x9504a0 + 1589);
        for (unsigned enabled : {0u, 1u, 255u})
            for (unsigned previous : {0u, 1u, 27u})
                for (bool dirty : {false, true}) {
                    State reference, modified, restored;
                    reference.reset(0, previous, dirty);
                    modified.reset(enabled, previous, dirty);
                    restored.reset(enabled, previous, dirty);
                    put(image, 0x2a61678, reference.cache.data());
                    probe_hero(image + 0x950760, reference.object.data(), reference.context.data());
                    ++calls;
                    apply(image, extent, identity);
                    put(image, 0x2a61678, modified.cache.data());
                    probe_hero(image + 0x950760, modified.object.data(), modified.context.data());
                    ++calls;
                    check(reference.outcome() == modified.outcome());
                    check(modified.object[0x18] == enabled);
                    rollback(image, extent, identity);
                    put(image, 0x2a61678, restored.cache.data());
                    probe_hero(image + 0x950760, restored.object.data(), restored.context.data());
                    ++calls;
                    check(restored.value == enabled);
                    check(std::memcmp(image + 0x9504a0, original_code.data(), 1589) == 0);
                }
        for (unsigned enabled : {0u, 1u, 255u})
            for (unsigned alignment = 0; alignment < 8; ++alignment) {
                std::array<unsigned char, 256> obj{}, zero{}, stream{}, reference_stream{};
                std::array<unsigned char, 512> output{}, expected{};
                obj.fill(0xa5);
                obj[0x18] = static_cast<unsigned char>(enabled);
                zero = obj;
                zero[0x18] = 0;
                put(stream.data(), 0x10, output.data() + alignment);
                put(stream.data(), 0x20, output.data() + output.size());
                put(reference_stream.data(), 0x10, expected.data() + alignment);
                put(reference_stream.data(), 0x20, expected.data() + expected.size());
                probe_hero(image + 0x9504a0, zero.data(), reference_stream.data());
                ++calls;
                apply(image, extent, identity);
                probe_hero(image + 0x9504a0, obj.data(), stream.data());
                ++calls;
                rollback(image, extent, identity);
                check(output == expected);
                check(get<unsigned char*>(stream.data(), 0x10) - output.data() ==
                      get<unsigned char*>(reference_stream.data(), 0x10) - expected.data());
                check(obj[0x18] == enabled);
                check(std::memcmp(image + 0x9504a0, original_code.data(), 1589) == 0);
            }
        rejects([&] { apply(image, extent, "wrong"); });
        rejects([&] { apply(image, site + 4, identity); });
        image[site] ^= 1;
        rejects([&] { apply(image, extent, identity); });
        image[site] ^= 1;
        apply(image, extent, identity);
        rejects([&] { apply(image, extent, identity); });
        rollback(image, extent, identity);
        DWORD protection{};
        VirtualProtect(image + 0x950000, 4096, PAGE_EXECUTE_READ, &protection);
        rejects([&] { apply(image, extent, identity); });
        VirtualProtect(image + 0x950000, 4096, protection, &protection);
        VirtualFree(image, 0, MEM_RELEASE);
        std::cout << "HERO_SHADOW_CONTROLS_VERIFIED assertions=" << checks << " original_cpu_calls=" << calls
                  << " shader_variants=1 visual_verified=false\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
