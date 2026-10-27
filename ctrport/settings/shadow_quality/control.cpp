// SPDX-License-Identifier: GPL-2.0-only
#define NOMINMAX
#include <windows.h>
#include "hook.hpp"
#include <array>
#include <vector>
#include <fstream>
#include <iostream>
#include <cstring>
#include <cmath>
#include <limits>
#include <thread>
#include <atomic>
using namespace ctrport::settings::shadow_quality;
extern "C" void probe_config(void*, void*, void*, int);
extern "C" float probe_publish(void*, void*, void*);
extern "C" void probe_viewport(void*, void*, void*, int);
static unsigned checks = 0, calls = 0;
void check(bool x) {
    ++checks;
    if (!x) throw std::runtime_error("shadow assertion failed");
}
template <class F> void rejects(F fn) {
    bool thrown = false;
    try {
        fn();
    } catch (const std::exception&) {
        thrown = true;
    }
    check(thrown);
}
template <class T, std::size_t N> void put(std::array<unsigned char, N>& b, std::size_t at, T value) {
    std::memcpy(b.data() + at, &value, sizeof(value));
}
template <class T, std::size_t N> T get(const std::array<unsigned char, N>& b, std::size_t at) {
    T v;
    std::memcpy(&v, b.data() + at, sizeof(v));
    return v;
}
constexpr auto sha = "39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5";
int main(int argc, char** argv) {
    try {
        check(argc == 2);
        constexpr std::size_t extent = 0x360000;
        auto* image = static_cast<unsigned char*>(
            VirtualAlloc(nullptr, extent, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        check(image != nullptr);
        std::ifstream f(argv[1], std::ios::binary);
        unsigned n{};
        f.read(reinterpret_cast<char*>(&n), 4);
        check(n == 3);
        for (unsigned i = 0; i < n; ++i) {
            unsigned at{}, size{};
            f.read(reinterpret_cast<char*>(&at), 4);
            f.read(reinterpret_cast<char*>(&size), 4);
            check(at + size < extent);
            f.read(reinterpret_cast<char*>(image + at), size);
            check(bool(f));
            image[at + size] = 0xc3;
        }
        std::vector<unsigned char> pristine(image, image + extent);
        check(!install(image, extent, sha, {}));
        rejects([&] { install(image, extent, "wrong", {Resolution::half, 0}); });
        rejects([&] { install(image, extent, sha, {Resolution::half, 4}); });
        rejects([&] { install(image, 0x3577a0, sha, {Resolution::half, 0}); });
        for (auto resolution : {Resolution::half, Resolution::three_quarters, Resolution::original})
            for (unsigned limit : {0u, 1u, 2u, 3u}) {
                if (resolution == Resolution::original && limit == 0) continue;
                for (int width : {1, 2, 31, 128, 1024, 2048, 4096})
                    for (float authored : {0.0f, 0.125f, 0.5f, 0.75f, 1.0f})
                        for (int slots : {1, 2, 4})
                            for (int requested : {1, 2, 3, 4}) {
                                std::array<unsigned char, 0x500> pass{}, original{}, state{}, expected_state{};
                                put(pass, 0x1f0, width);
                                put(state, 0x5c, authored);
                                put(state, 0x60, requested);
                                original = pass;
                                expected_state = state;
                                float factor = resolution == Resolution::half             ? 0.5f
                                               : resolution == Resolution::three_quarters ? 0.75f
                                                                                          : 1;
                                float candidate = (float(width) * authored) * factor;
                                const bool scales =
                                    resolution != Resolution::original && candidate >= 1 && std::isfinite(candidate);
                                const float expected_scale = scales ? authored * factor : authored;
                                put(expected_state, 0x5c, expected_scale);
                                if (limit && requested > int(limit)) put(expected_state, 0x60, int(limit));
                                probe_config(image + 0x357794, original.data(), expected_state.data(), slots);
                                ++calls;
                                const auto expected_published =
                                    probe_publish(image + 0x357072, original.data(), expected_state.data());
                                ++calls;
                                auto hook = install(image, extent, sha, {resolution, limit});
                                check(bool(hook));
                                probe_config(image + 0x357794, pass.data(), state.data(), slots);
                                ++calls;
                                const auto published = probe_publish(image + 0x357072, pass.data(), state.data());
                                ++calls;
                                check(pass == original);
                                check(published == expected_published);
                                check(get<float>(state, 0x5c) == authored);
                                check(get<int>(state, 0x60) == requested);
                                const auto count = get<int>(pass, 0x1ec);
                                check(count >= 1 && count <= slots && count <= requested);
                                for (int index = 0; index < count; ++index) {
                                    std::array<unsigned char, 0x100> camera{}, reference{};
                                    probe_viewport(image + 0x356f88, pass.data(), camera.data(), index);
                                    probe_viewport(image + 0x356f88, original.data(), reference.data(), index);
                                    calls += 2;
                                    check(camera == reference);
                                    if (authored <= 1) {
                                        check(get<int>(camera, 0x54) >= 0);
                                        check(get<int>(camera, 0x58) >= 0);
                                        check(get<int>(camera, 0x54) + get<int>(camera, 0x5c) <= width);
                                        check(get<int>(camera, 0x58) + get<int>(camera, 0x60) <= width * slots);
                                    }
                                }
                                hook->uninstall();
                                check(std::memcmp(image, pristine.data(), extent) == 0);
                            }
            }
        for (float special :
             {-1.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(), 1e-38f}) {
            std::array<unsigned char, 0x500> a{}, b{}, state{};
            put(a, 0x1f0, 1024);
            b = a;
            put(state, 0x5c, special);
            put(state, 0x60, 4);
            probe_config(image + 0x357794, a.data(), state.data(), 4);
            float old = probe_publish(image + 0x357072, a.data(), state.data());
            calls += 2;
            auto h = install(image, extent, sha, {Resolution::half, 0});
            probe_config(image + 0x357794, b.data(), state.data(), 4);
            float now = probe_publish(image + 0x357072, b.data(), state.data());
            calls += 2;
            check(a == b);
            check(std::memcmp(&old, &now, 4) == 0);
            h->uninstall();
        }
        for (std::size_t site : {std::size_t(0x357794), std::size_t(0x357072)}) {
            image[site] ^= 1;
            rejects([&] { install(image, extent, sha, {Resolution::half, 0}); });
            image[site] ^= 1;
            check(std::memcmp(image, pristine.data(), extent) == 0);
        }
        DWORD old{};
        VirtualProtect(image, extent, PAGE_EXECUTE_READ, &old);
        rejects([&] { install(image, extent, sha, {Resolution::half, 0}); });
        VirtualProtect(image, extent, PAGE_EXECUTE_READWRITE, &old);
        {
            auto h = install(image, extent, sha, {Resolution::half, 2});
            rejects([&] { install(image, extent, sha, {Resolution::half, 2}); });
            image[0x357794] ^= 1;
            rejects([&] { h->uninstall(); });
            image[0x357794] ^= 1;
            VirtualProtect(image, extent, PAGE_EXECUTE_READ, &old);
            h->uninstall();
            VirtualProtect(image, extent, PAGE_EXECUTE_READWRITE, &old);
            check(std::memcmp(image, pristine.data(), extent) == 0);
        }
        {
            auto h = install(image, extent, sha, {Resolution::half, 2});
            std::atomic<bool> okay{true};
            std::vector<std::thread> workers;
            for (unsigned thread = 0; thread < 8; ++thread)
                workers.emplace_back([&] {
                    for (unsigned repeat = 0; repeat < 100; ++repeat) {
                        std::array<unsigned char, 0x500> pass{}, state{};
                        put(pass, 0x1f0, 1024);
                        put(state, 0x5c, 1.f);
                        put(state, 0x60, 4);
                        probe_config(image + 0x357794, pass.data(), state.data(), 4);
                        const float value = probe_publish(image + 0x357072, pass.data(), state.data());
                        if (get<int>(pass, 0x1e8) != 512 || get<int>(pass, 0x1ec) != 2 || get<int>(pass, 0x1f4) != 4 ||
                            value != 0.5f || get<float>(state, 0x5c) != 1.f)
                            okay = false;
                    }
                });
            for (auto& worker : workers) worker.join();
            check(okay);
            calls += 1600;
            h->uninstall();
            check(std::memcmp(image, pristine.data(), extent) == 0);
        }
        VirtualFree(image, 0, MEM_RELEASE);
        std::cout << "SHADOW_QUALITY_CPU_VERIFIED assertions=" << checks << " original_block_calls=" << calls
                  << " visual_verified=false\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
