// SPDX-License-Identifier: GPL-2.0-only
#define NOMINMAX
#include <windows.h>
#include "override.hpp"
#include <vector>
#include <fstream>
#include <iostream>
#include <cstdint>
#include <cmath>
#include <limits>
extern "C" void vfx_producer_block(void*, void*, void*);
extern "C" std::uint64_t ctr_call_original_integer2(void*, std::uint64_t, std::uint64_t);
extern "C" void ctr_msvc_probe() {
    std::abort();
}
extern "C" void ctr_sysv_orbis_condition_broadcast() {
    std::abort();
}
extern "C" void ctr_thread_exit_frame() {
    std::abort();
}
namespace vfx = ctrport::settings::vfx;
constexpr const char* sha = "39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5";
unsigned checks = 0, calls = 0;
void require(bool b, const char* why) {
    ++checks;
    if (!b) throw std::runtime_error(why);
}
template <class T> void put(unsigned char* p, size_t o, T x) {
    std::memcpy(p + o, &x, sizeof(x));
}
template <class T> T get(const unsigned char* p, size_t o) {
    T x;
    std::memcpy(&x, p + o, sizeof(x));
    return x;
}
template <class F> void rejects(F f) {
    bool b = false;
    try {
        f();
    } catch (const std::invalid_argument&) {
        b = true;
    }
    require(b, "expected rejection");
}
struct Fixture {
    alignas(32) unsigned char parent[0x280]{}, scene[0x500]{}, bundles[0x200]{}, bundle[0x40]{}, enabled[0x20]{},
        list[0x30]{}, data[0x40]{};
    alignas(32) unsigned char context[0x50]{}, device[0x600]{}, manager[0x80]{}, caches[2][0x50]{}, values[2][0x10]{};
    void reset(unsigned char* image, float quality, int index, bool linked, int absent) {
        std::memset(this, 0, sizeof(*this));
        put(parent, 0x38, reinterpret_cast<uintptr_t>(list));
        put(parent, 0x50, reinterpret_cast<uintptr_t>(bundles));
        put(parent, 0x48, index);
        put(parent, 0x268, reinterpret_cast<uintptr_t>(enabled));
        put(bundles, 0x190 + 8 * index, reinterpret_cast<uintptr_t>(bundle));
        put(scene, 0x4f4, quality);
        put(list, 0x10, 4);
        put(list, 0x20, reinterpret_cast<uintptr_t>(data));
        put(bundle, 8, 7);
        put(context, 0x40, reinterpret_cast<uintptr_t>(device));
        put(device, 0x5a8, reinterpret_cast<uintptr_t>(manager));
        put(manager, 0x6c, 17);
        put(manager, 0x70, uintptr_t(0x12345678));
        for (int i = 0; i < 2; ++i) {
            put(caches[i], 0, reinterpret_cast<uintptr_t>(values[i]));
            put(caches[i], 0x38, linked ? uintptr_t(0x76543210) : 0);
            put(image, 0x2a62010 + 8 * i, (absent & (1 << i)) ? uintptr_t(0) : reinterpret_cast<uintptr_t>(caches[i]));
        }
    }
    void run(unsigned char* image) {
        auto before = std::vector<unsigned char>(scene, scene + sizeof(scene));
        vfx_producer_block(image + vfx::site, parent, scene);
        ++calls;
        require(std::memcmp(scene, before.data(), sizeof(scene)) == 0, "scene parameters mutated");
        require(get<int>(list, 0xc) == 1 && get<int>(bundle, 8) == 8, "list/refcount changed");
        require(get<uintptr_t>(data, 0) == reinterpret_cast<uintptr_t>(bundle), "wrong list registration");
        ctr_call_original_integer2(image + 0x967ba0, reinterpret_cast<uintptr_t>(bundle),
                                   reinterpret_cast<uintptr_t>(context));
        ++calls;
    }
    std::vector<unsigned char> snapshot() const {
        std::vector<unsigned char> s;
        for (auto p : {std::make_pair(bundle, sizeof(bundle)), std::make_pair(enabled, sizeof(enabled)),
                       std::make_pair(list, sizeof(list)), std::make_pair(data, sizeof(data)),
                       std::make_pair(manager, sizeof(manager)), std::make_pair(&caches[0][0], sizeof(caches)),
                       std::make_pair(&values[0][0], sizeof(values))})
            s.insert(s.end(), p.first, p.first + p.second);
        return s;
    }
};
LONG CALLBACK fault(EXCEPTION_POINTERS* e) {
    if (e->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) return EXCEPTION_CONTINUE_SEARCH;
    std::fprintf(stderr, "VFX_NATIVE_EXCEPTION code=%lx rip=%llx\n", e->ExceptionRecord->ExceptionCode,
                 e->ContextRecord->Rip);
    return EXCEPTION_CONTINUE_SEARCH;
}
int main(int argc, char** argv) {
    unsigned char* image = nullptr;
    try {
        require(argc == 2, "fixture argument");
        std::ifstream f(argv[1], std::ios::binary);
        require(bool(f), "fixture missing");
        constexpr size_t size = 0x2b00000;
        image = static_cast<unsigned char*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        require(image != nullptr, "allocation");
        uint32_t count = 0;
        f.read(reinterpret_cast<char*>(&count), 4);
        require(count == 4, "fixture spans");
        for (unsigned i = 0; i < count; ++i) {
            uint32_t a, n;
            f.read(reinterpret_cast<char*>(&a), 4);
            f.read(reinterpret_cast<char*>(&n), 4);
            require(a + n < size, "span extent");
            f.read(reinterpret_cast<char*>(image + a), n);
            require(bool(f), "span read");
        }
        image[0xaf05f0] = 0xc3; // Declared bounded-producer continuation; no called helper stub.
        DWORD old;
        require(VirtualProtect(image + 0xaf0000, 0x1000, PAGE_EXECUTE_READWRITE, &old) != 0, "code protection");
        require(VirtualProtect(image + 0x967000, 0x1000, PAGE_EXECUTE_READ, &old) != 0, "publisher protection");
        AddVectoredExceptionHandler(1, fault);
        Fixture fixture;
        for (float value : {0.f, 0.000001f, 0.25f, 1.f, -1.f, std::numeric_limits<float>::quiet_NaN()})
            for (int index : {0, 1})
                for (bool linked : {false, true})
                    for (int absent : {0, 1, 2, 3}) {
                        fixture.reset(image, value, index, linked, absent);
                        fixture.run(image);
                        const auto original = fixture.snapshot();
                        for (auto q : {vfx::Quality::low, vfx::Quality::high}) {
                            fixture.reset(image, q == vfx::Quality::low ? 0.f : 1.f, index, linked, absent);
                            fixture.run(image);
                            const auto reference = fixture.snapshot();
                            vfx::apply(image, size, sha, q);
                            FlushInstructionCache(GetCurrentProcess(), image + vfx::site, 9);
                            fixture.reset(image, value, index, linked, absent);
                            fixture.run(image);
                            require(fixture.snapshot() == reference, "override differs from original quality input");
                            require(get<float>(fixture.bundle, 0x18) == (q == vfx::Quality::low ? 0.f : 1.f),
                                    "quality not propagated");
                            if (!(absent & 1))
                                require(get<float>(fixture.values[0], 0) == get<float>(fixture.bundle, 0x18),
                                        "quality publisher mismatch");
                            if (!(absent & 2))
                                require(get<uint32_t>(fixture.values[1], 0) == fixture.bundle[0x1c],
                                        "edge enable publisher mismatch");
                            vfx::rollback(image, size, sha, q);
                            FlushInstructionCache(GetCurrentProcess(), image + vfx::site, 9);
                            fixture.reset(image, value, index, linked, absent);
                            fixture.run(image);
                            require(fixture.snapshot() == original, "original rollback mismatch");
                        }
                    }
        rejects([&] { vfx::apply(image, size, "wrong", vfx::Quality::low); });
        rejects([&] { vfx::apply(image, vfx::site + 8, sha, vfx::Quality::low); });
        image[vfx::site] ^= 1;
        rejects([&] { vfx::apply(image, size, sha, vfx::Quality::high); });
        image[vfx::site] ^= 1;
        vfx::verify(image, size, sha, vfx::Quality::original);
        require(std::memcmp(image + vfx::site, vfx::original.data(), 9) == 0, "code restored");
        VirtualFree(image, 0, MEM_RELEASE);
        std::cout << "VFX_NATIVE_VERIFIED checks=" << checks << " original_calls=" << calls
                  << " bounded_producer=true visual=false\n";
        return 0;
    } catch (const std::exception& e) {
        if (image) VirtualFree(image, 0, MEM_RELEASE);
        std::cerr << e.what() << "\n";
        return 1;
    }
}
