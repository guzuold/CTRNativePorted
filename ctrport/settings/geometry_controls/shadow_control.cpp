// SPDX-License-Identifier: GPL-2.0-only
#define NOMINMAX
#include <windows.h>
#include "shadow_override.hpp"
#include <array>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
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
namespace gc = ctrport::settings::geometry_controls;
namespace {
unsigned checks = 0, calls = 0;
std::uint64_t boundary_address = 0;
void require(bool v, const char* why) {
    ++checks;
    if (!v) throw std::runtime_error(why);
}
template <class T> void put(unsigned char* p, std::size_t off, T v) {
    std::memcpy(p + off, &v, sizeof(v));
}
template <class T> T get(const unsigned char* p, std::size_t off) {
    T v{};
    std::memcpy(&v, p + off, sizeof(v));
    return v;
}
LONG CALLBACK boundary(EXCEPTION_POINTERS* e) {
    if (e->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || e->ContextRecord->Rip != boundary_address)
        return EXCEPTION_CONTINUE_SEARCH;
    static constexpr char marker[] = "SHADOW_ACTIVE_SPLIT_BOUNDARY\n";
    DWORD written = 0;
    const bool ok = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), marker, sizeof(marker) - 1, &written, nullptr) != 0;
    ExitProcess(ok && written == sizeof(marker) - 1 ? 42 : 43);
}
struct Fixture {
    std::array<unsigned char, 0x500> pass{};
    std::array<unsigned char, 0x80> context{}, params{}, light{}, parameter_handle{}, light_handle{}, target{},
        attachment_list{}, attachment{}, camera_list{};
    std::array<std::array<unsigned char, 0x100>, 4> cameras{};
    std::array<std::uintptr_t, 4> camera_entries{};
    std::uintptr_t attachment_entry = 0;
    void setup(bool enabled, bool null_camera) {
        put(pass.data(), 0x1e0, reinterpret_cast<std::uintptr_t>(parameter_handle.data()));
        put(pass.data(), 0x1d8, reinterpret_cast<std::uintptr_t>(light_handle.data()));
        put(parameter_handle.data(), 0x28, reinterpret_cast<std::uintptr_t>(params.data()));
        put(light_handle.data(), 0x28, reinterpret_cast<std::uintptr_t>(light.data()));
        put(params.data(), 8, std::uint32_t(100));
        put(light.data(), 8, std::uint32_t(100));
        params[0x64] = enabled ? 1 : 0;
        light[0x40] = 2;
        put(light.data(), 0x50, 1.0f);
        put(params.data(), 0x5c, 1.0f);
        put(params.data(), 0x60, std::int32_t(4));
        pass[0x1c0] = 1;
        put(pass.data(), 0x130, std::uintptr_t(1)); // never dereferenced on disabled path
        put(pass.data(), 0x48, reinterpret_cast<std::uintptr_t>(target.data()));
        put(target.data(), 0x10, reinterpret_cast<std::uintptr_t>(attachment_list.data()));
        put(attachment_list.data(), 0xc, std::uint32_t(1));
        attachment_entry = reinterpret_cast<std::uintptr_t>(attachment.data());
        put(attachment_list.data(), 0x20, reinterpret_cast<std::uintptr_t>(&attachment_entry));
        put(attachment.data(), 0x58, std::int32_t(512));
        put(attachment.data(), 0x5c, std::int32_t(2048));
        for (unsigned i = 0; i < 4; ++i) {
            cameras[i][0xf2] = 1;
            camera_entries[i] = (null_camera && i == 2) ? 0 : reinterpret_cast<std::uintptr_t>(cameras[i].data());
        }
        put(camera_list.data(), 0x20, reinterpret_cast<std::uintptr_t>(camera_entries.data()));
        put(pass.data(), 0x3c0, reinterpret_cast<std::uintptr_t>(camera_list.data()));
    }
    void call(unsigned char* image) {
        ctr_call_original_integer2(image + 0x357540, reinterpret_cast<std::uintptr_t>(pass.data()),
                                   reinterpret_cast<std::uintptr_t>(context.data()));
        ++calls;
    }
    void verify(bool null_camera) {
        require(get<std::int32_t>(pass.data(), 0x1ec) == -1, "disabled cascade count");
        for (unsigned i = 0; i < 4; ++i)
            require(cameras[i][0xf2] == (null_camera && i == 2 ? 1 : 0), "native shadow camera disabled flag");
        require(get<std::uint32_t>(params.data(), 8) == 100 && get<std::uint32_t>(light.data(), 8) == 100,
                "original reference cleanup retained");
    }
};
}
int wmain(int argc, wchar_t** argv) {
    unsigned char* image = nullptr;
    try {
        if (argc < 2 || argc > 3) return 2;
        constexpr std::size_t extent = 0x2b00000;
        image = static_cast<unsigned char*>(VirtualAlloc(nullptr, extent, MEM_RESERVE, PAGE_NOACCESS));
        require(image != nullptr, "reserve");
        std::ifstream in(argv[1], std::ios::binary);
        std::uint32_t n = 0;
        in.read(reinterpret_cast<char*>(&n), 4);
        require(n == 10, "fixture count");
        for (unsigned i = 0; i < n; ++i) {
            std::uint32_t a = 0, s = 0;
            in.read(reinterpret_cast<char*>(&a), 4);
            in.read(reinterpret_cast<char*>(&s), 4);
            require(a < extent && s <= extent - a, "bounds");
            const auto lo = a & ~0xfffu, hi = (a + s + 4095) & ~0xfffu;
            require(VirtualAlloc(image + lo, hi - lo, MEM_COMMIT, PAGE_EXECUTE_READWRITE) != nullptr, "commit");
            in.read(reinterpret_cast<char*>(image + a), s);
            require(bool(in), "read");
        }
        require(VirtualAlloc(image + 0x2807000, 4096, MEM_COMMIT, PAGE_READWRITE) != nullptr, "guard commit");
        std::uint64_t guard = 0x1234;
        put(image, 0x2807098, reinterpret_cast<std::uintptr_t>(&guard));
        if (argc == 3) {
            boundary_address = reinterpret_cast<std::uintptr_t>(image) + 0x3557b0;
            AddVectoredExceptionHandler(1, boundary);
            Fixture active;
            active.setup(true, false);
            active.call(image);
            throw std::runtime_error("active branch failed to reach split computation");
        }
        constexpr auto sha = "39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5";
        for (bool missing : {false, true}) {
            Fixture original;
            original.setup(false, missing);
            original.call(image);
            original.verify(missing);
            gc::apply_shadow_off(image, extent, sha);
            FlushInstructionCache(GetCurrentProcess(), image + gc::shadow_site, 2);
            Fixture overridden;
            overridden.setup(true, missing);
            overridden.call(image);
            overridden.verify(missing);
            require(overridden.params[0x64] == 1, "override mutated scene-owned setting");
            require(overridden.cameras == original.cameras,
                    "override differs from native scene-disabled camera outputs");
            require(get<std::int32_t>(overridden.pass.data(), 0x1ec) == get<std::int32_t>(original.pass.data(), 0x1ec),
                    "native count mismatch");
            gc::rollback_shadow(image, extent, sha);
            FlushInstructionCache(GetCurrentProcess(), image + gc::shadow_site, 2);
        }
        unsigned rejected = 0;
        try {
            gc::apply_shadow_off(image, extent, "wrong");
        } catch (const std::invalid_argument&) {
            ++rejected;
        }
        try {
            gc::apply_shadow_off(image, gc::shadow_site + 1, sha);
        } catch (const std::invalid_argument&) {
            ++rejected;
        }
        image[gc::shadow_site + 1] ^= 1;
        try {
            gc::apply_shadow_off(image, extent, sha);
        } catch (const std::invalid_argument&) {
            ++rejected;
        }
        image[gc::shadow_site + 1] ^= 1;
        require(rejected == 3 && image[gc::shadow_site] == 0x74, "negative preflight mutated original");
        VirtualFree(image, 0, MEM_RELEASE);
        image = nullptr;
        std::cout << "GEOMETRY_SHADOW_CPU_VERIFIED checks=" << checks << " original_cpu_calls=" << calls
                  << " allocations_executed=false visual_verified=false\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        if (image) VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
}
