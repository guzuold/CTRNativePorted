// SPDX-License-Identifier: GPL-2.0-only
#define NOMINMAX
#include <windows.h>
#include "../../retail_map.hpp"
#include "hook.hpp"
#include <array>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <cstdint>
namespace ctrport::settings::shadow_quality {
namespace {
constexpr std::size_t configure = 0x357794, publisher = 0x357072;
constexpr std::array<unsigned char, 37> original_config{
    {0xc4, 0xc1, 0x7a, 0x59, 0x45, 0x5c, 0xc5, 0xfa, 0x2c, 0xc8, 0x89, 0x8b, 0xe8, 1,    0,    0, 0x89, 0x83, 0xf4,
     1,    0,    0,    0x41, 0x8b, 0x4d, 0x60, 0x39, 0xc8, 0x0f, 0x4e, 0xc8, 0x89, 0x8b, 0xec, 1, 0,    0}};
constexpr std::array<unsigned char, 20> original_publish{
    {0xc5, 0xfa, 0x10, 0x43, 0x5c, 0x41, 0x83, 0xbf, 0xec, 1, 0, 0, 0, 0x48, 0x89, 0x9d, 0xc0, 0xfe, 0xff, 0xff}};
void jump(unsigned char* p, const void* to) {
    const unsigned char op[]{0xff, 0x25, 0, 0, 0, 0};
    std::memcpy(p, op, 6);
    std::memcpy(p + 6, &to, 8);
}
void writable(unsigned char* p, std::size_t n) {
    MEMORY_BASIC_INFORMATION r{};
    if (!VirtualQuery(p, &r, sizeof(r)) || r.State != MEM_COMMIT || (r.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
        !(r.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)) ||
        reinterpret_cast<std::uintptr_t>(p) + n > reinterpret_cast<std::uintptr_t>(r.BaseAddress) + r.RegionSize)
        throw std::runtime_error("shadow quality requires writable bootstrap image");
}
struct Emitter {
    std::vector<unsigned char> b;
    std::size_t origin;
    void emit(std::initializer_list<unsigned char> x) { b.insert(b.end(), x); }
    void literal(std::initializer_list<unsigned char> op, std::size_t address) {
        emit(op);
        const auto disp = static_cast<std::int32_t>(address - origin - b.size() - 4);
        const auto* p = reinterpret_cast<const unsigned char*>(&disp);
        b.insert(b.end(), p, p + 4);
    }
    void scale(bool pub) {
        emit({0x48, 0x83, 0xec, 0x28, 0xc5, 0xfc, 0x11, 0x0c, 0x24});
        if (pub) {
            emit({0xc4, 0xc1, 0x7a, 0x2a, 0x8f, 0xf0, 1, 0, 0, 0xc5, 0xf2, 0x59, 0xc8});
            literal({0xc5, 0xf2, 0x59, 0x0d}, 0x300);
        } else
            literal({0xc5, 0xfa, 0x59, 0x0d}, 0x300);
        literal({0xc5, 0xf8, 0x2e, 0x0d}, 0x304);
        emit({0x72, 0});
        auto low = b.size() - 1; // unordered or <1 -> unchanged
        literal({0xc5, 0xf8, 0x2e, 0x0d}, 0x308);
        emit({0x77, 0});
        auto high = b.size() - 1; // infinity -> unchanged
        if (pub)
            literal({0xc5, 0xfa, 0x59, 0x05}, 0x300);
        else
            emit({0xc5, 0xfa, 0x10, 0xc1});
        b[low] = static_cast<unsigned char>(b.size() - low - 1);
        b[high] = static_cast<unsigned char>(b.size() - high - 1);
        emit({0xc5, 0xfc, 0x10, 0x0c, 0x24, 0x48, 0x83, 0xc4, 0x28});
    }
    void finish(unsigned char* memory, const void* to) {
        const auto n = b.size();
        b.resize(n + 14);
        jump(b.data() + n, to);
        if (b.size() > 0x100) throw std::runtime_error("shadow thunk overflow");
        std::memcpy(memory + origin, b.data(), b.size());
    }
};
}
std::unique_ptr<Hook> install(unsigned char* base, std::size_t extent, const std::string& sha, Options opt) {
    if (opt.cascade_limit > 3 || (opt.resolution != Resolution::original && opt.resolution != Resolution::half &&
                                  opt.resolution != Resolution::three_quarters))
        throw std::invalid_argument("unsupported shadow quality");
    if (!opt.cascade_limit && opt.resolution == Resolution::original) return {};
    const bool pub = opt.resolution != Resolution::original;
    if (!retail_map::bound(sha) || !base || !retail_map::has(configure) || (pub && !retail_map::has(publisher)) ||
        retail_map::at(configure) > extent || original_config.size() > extent - retail_map::at(configure))
        throw std::invalid_argument("unsupported shadow image");
    writable(base + retail_map::at(configure), original_config.size());
    if (!retail_map::same(base + retail_map::at(configure), configure, original_config.data(), original_config.size(),
                          true))
        throw std::runtime_error("shadow configure signature mismatch");
    if (pub) {
        writable(base + retail_map::at(publisher), original_publish.size());
        if (!retail_map::same(base + retail_map::at(publisher), publisher, original_publish.data(),
                              original_publish.size(), true))
            throw std::runtime_error("shadow publisher signature mismatch");
    }
    auto h = std::unique_ptr<Hook>(new Hook());
    h->image_ = base;
    h->publisher_ = pub;
    h->code_ = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!h->code_) throw std::runtime_error("shadow thunk allocation failed");
    const float constants[]{opt.resolution == Resolution::half ? 0.5f : 0.75f, 1.0f, 3.402823466e38f};
    std::memcpy(h->code_ + 0x300, constants, sizeof(constants));
    Emitter c{{}, 0};
    c.b.insert(c.b.end(), original_config.begin(), original_config.begin() + 6);
    if (pub) c.scale(false);
    c.b.insert(c.b.end(), original_config.begin() + 6, original_config.begin() + 31);
    if (opt.cascade_limit) {
        c.emit({0x9c, 0x83, 0xf9, static_cast<unsigned char>(opt.cascade_limit), 0x7e, 5, 0xb9,
                static_cast<unsigned char>(opt.cascade_limit), 0, 0, 0, 0x9d});
    }
    c.b.insert(c.b.end(), original_config.begin() + 31, original_config.end());
    c.finish(h->code_, base + retail_map::at(configure) + original_config.size());
    if (pub) {
        Emitter p{{}, 0x100};
        p.b.insert(p.b.end(), original_publish.begin(), original_publish.begin() + 5);
        p.scale(true);
        p.b.insert(p.b.end(), original_publish.begin() + 5, original_publish.end());
        p.finish(h->code_, base + retail_map::at(publisher) + original_publish.size());
    }
    DWORD old{};
    if (!VirtualProtect(h->code_, 4096, PAGE_EXECUTE_READ, &old))
        throw std::runtime_error("shadow thunk protection failed");
    FlushInstructionCache(GetCurrentProcess(), h->code_, 4096);
    jump(base + retail_map::at(configure), h->code_);
    std::memset(base + retail_map::at(configure) + 14, 0x90, original_config.size() - 14);
    if (pub) {
        jump(base + retail_map::at(publisher), h->code_ + 0x100);
        std::memset(base + retail_map::at(publisher) + 14, 0x90, original_publish.size() - 14);
    }
    h->installed_ = true;
    FlushInstructionCache(GetCurrentProcess(), base + retail_map::at(configure), original_config.size());
    if (pub) FlushInstructionCache(GetCurrentProcess(), base + retail_map::at(publisher), original_publish.size());
    return h;
}
void Hook::uninstall() {
    if (!installed_) return;
    auto ec = original_config;
    ec.fill(0x90);
    jump(ec.data(), code_);
    auto ep = original_publish;
    ep.fill(0x90);
    jump(ep.data(), code_ + 0x100);
    if (std::memcmp(image_ + retail_map::at(configure), ec.data(), ec.size()) ||
        (publisher_ && std::memcmp(image_ + retail_map::at(publisher), ep.data(), ep.size())))
        throw std::runtime_error("shadow hook modified by another patch");
    DWORD oldc{}, oldp{};
    if (!VirtualProtect(image_ + retail_map::at(configure), ec.size(), PAGE_EXECUTE_READWRITE, &oldc))
        throw std::runtime_error("shadow restore protection failed");
    if (publisher_ && !VirtualProtect(image_ + retail_map::at(publisher), ep.size(), PAGE_EXECUTE_READWRITE, &oldp)) {
        DWORD ignored{};
        VirtualProtect(image_ + retail_map::at(configure), ec.size(), oldc, &ignored);
        throw std::runtime_error("shadow publisher restore protection failed");
    }
    std::memcpy(image_ + retail_map::at(configure), original_config.data(), original_config.size());
    if (publisher_) std::memcpy(image_ + retail_map::at(publisher), original_publish.data(), original_publish.size());
    FlushInstructionCache(GetCurrentProcess(), image_ + retail_map::at(configure), ec.size());
    if (publisher_) FlushInstructionCache(GetCurrentProcess(), image_ + retail_map::at(publisher), ep.size());
    DWORD ignored{};
    const bool restored_p =
        !publisher_ || VirtualProtect(image_ + retail_map::at(publisher), ep.size(), oldp, &ignored);
    const bool restored_c = VirtualProtect(image_ + retail_map::at(configure), ec.size(), oldc, &ignored) != 0;
    installed_ = false;
    if (!restored_p || !restored_c) throw std::runtime_error("shadow restore original protection failed");
}
Hook::~Hook() {
    try {
        uninstall();
    } catch (...) {
        std::terminate();
    }
    if (code_) VirtualFree(code_, 0, MEM_RELEASE);
}
}
