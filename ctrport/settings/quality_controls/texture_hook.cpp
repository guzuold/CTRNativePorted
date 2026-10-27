// SPDX-License-Identifier: GPL-2.0-only
#define NOMINMAX
#include <windows.h>
#include "../../retail_map.hpp"
#include "texture_hook.hpp"
#include <array>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <exception>
#include <vector>
namespace ctrport::settings::quality_controls {
namespace {
constexpr std::size_t site = 0x141be9c, size = 16;
constexpr unsigned char original[]{0x48, 0x8b, 0x83, 0x90, 0,    0,    0,    0x48,
                                   0x85, 0xc0, 0x74, 0x08, 0x0f, 0xbe, 0x40, 0x63};
void jump(unsigned char* p, const void* target) {
    const unsigned char op[]{0xff, 0x25, 0, 0, 0, 0};
    std::memcpy(p, op, 6);
    std::memcpy(p + 6, &target, 8);
}
}
std::unique_ptr<TextureHook> install_texture(unsigned char* base, std::size_t extent, const std::string& sha,
                                             TextureMips mode) {
    if (mode != TextureMips::original && mode != TextureMips::half && mode != TextureMips::quarter)
        throw std::invalid_argument("invalid texture mip setting");
    if (mode == TextureMips::original) return {};
    if (!base || !retail_map::bound(sha) || !retail_map::has(site) || retail_map::at(site) > extent ||
        size > extent - retail_map::at(site))
        throw std::invalid_argument("unsupported texture hook image");
    MEMORY_BASIC_INFORMATION region{};
    if (!VirtualQuery(base + retail_map::at(site), &region, sizeof(region)) || region.State != MEM_COMMIT ||
        (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
        !(region.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)) ||
        reinterpret_cast<std::uintptr_t>(base + retail_map::at(site)) + size >
            reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize)
        throw std::invalid_argument("texture hook requires writable bootstrap image");
    if (!retail_map::same(base + retail_map::at(site), site, original, size, true))
        throw std::invalid_argument("texture hook signature mismatch");
    auto h = std::unique_ptr<TextureHook>(new TextureHook());
    h->image_ = base;
    h->code_ = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!h->code_) throw std::runtime_error("texture thunk allocation");
    // Register-only fragment: reproduce null predicates, preserve all registers
    // except RAX and flags (the original load/test outputs). Preserve file params.
    std::vector<unsigned char> b{0x48, 0x8b, 0x83, 0x90, 0, 0, 0,    0x48, 0x85, 0xc0,
                                 0x0f, 0x84, 0,    0,    0, 0, 0x0f, 0xbe, 0x40, 0x63};
    const auto count = static_cast<unsigned char>(mode == TextureMips::half ? 1 : 2);
    // Only ordinary 2D mip chains. Small UI/data images, cubes, volumes and
    // single-level images retain authored behavior; native larger drops retained.
    std::vector<std::size_t> keep;
    b.insert(b.end(), {0x85, 0xc0, 0x0f, 0x88, 0, 0, 0, 0});
    keep.push_back(b.size() - 4); // preserve negative file sentinels
    auto cond = [&](std::initializer_list<unsigned char> op) {
        b.insert(b.end(), op);
        b.insert(b.end(), {0x0f, 0x85, 0, 0, 0, 0});
        keep.push_back(b.size() - 4);
    };
    cond({0x66, 0x41, 0x83, 0x7e, 0x1c, 0x01}); // depth == 1
    cond({0x66, 0x41, 0x83, 0x7e, 0x20, 0x01}); // imageCount == 1
    auto below = [&](std::initializer_list<unsigned char> op) {
        b.insert(b.end(), op);
        b.insert(b.end(), {0x0f, 0x82, 0, 0, 0, 0});
        keep.push_back(b.size() - 4);
    };
    below({0x66, 0x41, 0x81, 0x7e, 0x18, 0x00, 0x01}); // width >=256
    below({0x66, 0x41, 0x81, 0x7e, 0x1a, 0x00, 0x01}); // height >=256
    b.insert(b.end(), {0x83, 0xf8, count, 0x0f, 0x8d, 0, 0, 0, 0});
    keep.push_back(b.size() - 4);
    b.insert(b.end(), {0xb8, count, 0, 0, 0});
    const auto keep_at = b.size();
    b.insert(b.end(), {0x85, 0xc0});
    const auto resume = b.size();
    b.resize(b.size() + 14);
    jump(b.data() + resume, base + retail_map::at(site) + 0x12);
    const auto done = b.size();
    b.resize(b.size() + 14);
    jump(b.data() + done, base + retail_map::at(site) + 0x14);
    auto rel = [&](std::size_t at, std::size_t target) {
        auto d = static_cast<std::int32_t>(target - (at + 4));
        std::memcpy(b.data() + at, &d, 4);
    };
    rel(12, done);
    for (auto at : keep) rel(at, keep_at);
    std::memcpy(h->code_, b.data(), b.size());
    DWORD old{};
    if (!VirtualProtect(h->code_, 4096, PAGE_EXECUTE_READ, &old)) throw std::runtime_error("texture thunk protect");
    FlushInstructionCache(GetCurrentProcess(), h->code_, 4096);
    jump(base + retail_map::at(site), h->code_);
    std::memset(base + retail_map::at(site) + 14, 0x90, size - 14);
    h->installed_ = true;
    FlushInstructionCache(GetCurrentProcess(), base + retail_map::at(site), size);
    return h;
}
void TextureHook::uninstall() {
    if (!installed_) return;
    std::array<unsigned char, size> expected{};
    jump(expected.data(), code_);
    std::memset(expected.data() + 14, 0x90, size - 14);
    if (std::memcmp(image_ + retail_map::at(site), expected.data(), size))
        throw std::runtime_error("texture hook altered");
    DWORD old{};
    if (!VirtualProtect(image_ + retail_map::at(site), size, PAGE_EXECUTE_READWRITE, &old))
        throw std::runtime_error("texture unhook protect");
    std::memcpy(image_ + retail_map::at(site), original, size);
    FlushInstructionCache(GetCurrentProcess(), image_ + retail_map::at(site), size);
    DWORD ignored{};
    if (!VirtualProtect(image_ + retail_map::at(site), size, old, &ignored))
        throw std::runtime_error("texture unhook restore");
    installed_ = false;
}
TextureHook::~TextureHook() {
    try {
        uninstall();
    } catch (...) {
        std::terminate();
    }
    if (code_) VirtualFree(code_, 0, MEM_RELEASE);
}
}
