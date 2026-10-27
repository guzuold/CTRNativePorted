// SPDX-License-Identifier: GPL-2.0-only
#define NOMINMAX
#include <windows.h>
#include "../../retail_map.hpp"
#include "world_hook.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <exception>
#include <vector>
extern "C" std::uint64_t ctr_call_original_registers(void*, const std::uint64_t*, const std::uint64_t*, std::uint64_t*);
namespace ctrport::settings::quality_controls {
namespace {
constexpr std::size_t site = 0x348350, size = 17, trampoline = 0x100;
constexpr unsigned char original[size] = {0x55, 0x48, 0x89, 0xe5, 0x41, 0x57, 0x41, 0x56, 0x41,
                                          0x55, 0x41, 0x54, 0x53, 0x48, 0x83, 0xec, 0x48};
template <class T> T read(const unsigned char* p, std::size_t at) {
    T v;
    std::memcpy(&v, p + at, sizeof(v));
    return v;
}
void absolute_jump(unsigned char* p, const void* target) {
    const unsigned char op[]{0xff, 0x25, 0, 0, 0, 0};
    std::memcpy(p, op, 6);
    std::memcpy(p + 6, &target, 8);
}
extern "C" void world_callback(WorldHook* owner, const unsigned long long* args) noexcept {
    owner->invoke(args);
}
// Copies guest memory, false when it is not readable. Runs for every world draw, so it must not
// make a system call (VirtualQuery here cost three per draw): the read is guarded instead.
bool copy(void* to, const void* from, std::size_t bytes) noexcept {
    if (!from) return false;
    __try {
        std::memcpy(to, from, bytes);
        return true;
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER
                                                                  : EXCEPTION_CONTINUE_SEARCH) {
        return false;
    }
}
// draw, data and alternate are local copies of the guest objects.
bool eligible(const unsigned char* draw, const unsigned char* data, const unsigned char* alternate) noexcept {
    if (read<std::uint64_t>(draw, 0x28) || read<std::uint64_t>(draw, 0x30) ||
        read<std::int32_t>(data, 0x78) != -1)
        return false;
    const auto count = read<std::uint32_t>(alternate, 0x20), original_count = read<std::uint32_t>(data, 0x68);
    return count > 0 && count < original_count && read<std::uint64_t>(alternate, 0x30) != 0 &&
           read<std::uint64_t>(alternate, 0x40) != 0 && read<std::uint64_t>(alternate, 0x38) != 0 &&
           read<std::uint64_t>(alternate, 0x38) == read<std::uint64_t>(data, 0x88) &&
           (read<std::uint32_t>(alternate, 0x24) & 63) == (read<std::uint32_t>(data, 0x6c) & 63);
}
}
void WorldHook::invoke(const unsigned long long* arguments) noexcept {
    std::array<std::uint64_t, 6> args{};
    std::memcpy(args.data(), arguments, 2 * sizeof(std::uint64_t));
    alignas(16) std::array<unsigned char, 0x80> call_draw{};
    alignas(16) std::array<unsigned char, 0x98> call_data{};
    std::array<unsigned char, 0x48> alternate{};
    if (copy(call_draw.data(), reinterpret_cast<const void*>(args[1]), call_draw.size()) &&
        copy(call_data.data(), read<const unsigned char*>(call_draw.data(), 0x10), call_data.size()) &&
        copy(alternate.data(), read<const unsigned char*>(call_data.data(), 0x58), alternate.size()) &&
        eligible(call_draw.data(), call_data.data(), alternate.data())) {
        const auto* replacement = call_data.data();
        std::memcpy(call_draw.data() + 0x10, &replacement, sizeof(replacement));
        std::memcpy(call_data.data() + 0x68, alternate.data() + 0x20, 4);
        std::memcpy(call_data.data() + 0x80, alternate.data() + 0x30, 8);
        std::memcpy(call_data.data() + 0x90, alternate.data() + 0x40, 8);
        args[1] = reinterpret_cast<std::uintptr_t>(call_draw.data());
    }
    std::array<std::uint64_t, 8> fp{};
    std::uint64_t result{};
    ctr_call_original_registers(code_ + trampoline, args.data(), fp.data(), &result);
}
std::unique_ptr<WorldHook> install_world(unsigned char* base, std::size_t extent, const std::string& sha,
                                         WorldLod selected) {
    if (selected != WorldLod::original && selected != WorldLod::reduced)
        throw std::invalid_argument("invalid world mesh setting");
    if (selected == WorldLod::original) return {};
    if (!retail_map::bound(sha) || !base || !retail_map::has(site) || retail_map::at(site) > extent ||
        size > extent - retail_map::at(site))
        throw std::invalid_argument("unsupported world mesh hook image");
    MEMORY_BASIC_INFORMATION region{};
    if (!VirtualQuery(base + retail_map::at(site), &region, sizeof(region)) || region.State != MEM_COMMIT ||
        (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
        !(region.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)) ||
        reinterpret_cast<std::uintptr_t>(base + retail_map::at(site)) + size >
            reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize)
        throw std::invalid_argument("world mesh hook requires writable bootstrap image");
    if (!retail_map::same(base + retail_map::at(site), site, original, size, true))
        throw std::invalid_argument("world encoder prologue mismatch");
    auto hook = std::unique_ptr<WorldHook>(new WorldHook());
    hook->image_ = base;
    hook->code_ = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!hook->code_) throw std::runtime_error("world mesh thunk allocation failed");
    // SysV arguments -> call-local array after Win64 shadow space. Subtracting
    // 88 bytes aligns the native Win64 call from the SysV entry alignment.
    std::vector<unsigned char> stub{0x48, 0x83, 0xec, 0x58, 0x48, 0x89, 0x7c, 0x24, 0x20, 0x48, 0x89, 0x74,
                                    0x24, 0x28, 0x48, 0x89, 0x54, 0x24, 0x30, 0x48, 0x89, 0x4c, 0x24, 0x38,
                                    0x4c, 0x89, 0x44, 0x24, 0x40, 0x48, 0x8d, 0x54, 0x24, 0x20, 0x48, 0xb9};
    auto append_pointer = [&](const void* p) {
        const auto* b = reinterpret_cast<const unsigned char*>(&p);
        stub.insert(stub.end(), b, b + 8);
    };
    append_pointer(hook.get());
    stub.insert(stub.end(), {0x48, 0xb8});
    append_pointer(reinterpret_cast<const void*>(&world_callback));
    stub.insert(stub.end(), {0xff, 0xd0, 0x48, 0x83, 0xc4, 0x58, 0xc3});
    std::memcpy(hook->code_, stub.data(), stub.size());
    std::memcpy(hook->code_ + trampoline, original, size);
    absolute_jump(hook->code_ + trampoline + size, base + retail_map::at(site) + size);
    // UNWIND_INFO: v1, prolog4, one UWOP_ALLOC_SMALL(size88).
    const unsigned char unwind[]{1, 4, 1, 0, 4, 0xa2, 0, 0};
    std::memcpy(hook->code_ + 0x200, unwind, sizeof(unwind));
    auto* function = reinterpret_cast<RUNTIME_FUNCTION*>(hook->code_ + 0x210);
    *function = {0, static_cast<DWORD>(stub.size()), 0x200};
    if (!RtlAddFunctionTable(function, 1, reinterpret_cast<DWORD64>(hook->code_)))
        throw std::runtime_error("world mesh unwind registration failed");
    hook->unwind_registered_ = true;
    DWORD previous = 0;
    if (!VirtualProtect(hook->code_, 4096, PAGE_EXECUTE_READ, &previous))
        throw std::runtime_error("world mesh thunk protection failed");
    FlushInstructionCache(GetCurrentProcess(), hook->code_, 4096);
    absolute_jump(base + retail_map::at(site), hook->code_);
    std::memset(base + retail_map::at(site) + 14, 0x90, size - 14);
    hook->installed_ = true;
    FlushInstructionCache(GetCurrentProcess(), base + retail_map::at(site), size);
    return hook;
}
void WorldHook::uninstall() {
    if (!installed_) return;
    std::array<unsigned char, size> expected{};
    absolute_jump(expected.data(), code_);
    std::memset(expected.data() + 14, 0x90, size - 14);
    if (std::memcmp(image_ + retail_map::at(site), expected.data(), size))
        throw std::runtime_error("world mesh hook altered by another patch");
    DWORD old = 0;
    if (!VirtualProtect(image_ + retail_map::at(site), size, PAGE_EXECUTE_READWRITE, &old))
        throw std::runtime_error("world mesh unhook protection failed");
    std::memcpy(image_ + retail_map::at(site), original, size);
    FlushInstructionCache(GetCurrentProcess(), image_ + retail_map::at(site), size);
    DWORD ignored = 0;
    if (!VirtualProtect(image_ + retail_map::at(site), size, old, &ignored))
        throw std::runtime_error("world mesh unhook restore protection failed");
    installed_ = false;
}
WorldHook::~WorldHook() {
    try {
        uninstall();
    } catch (...) {
        std::terminate();
    }
    if (unwind_registered_) RtlDeleteFunctionTable(reinterpret_cast<RUNTIME_FUNCTION*>(code_ + 0x210));
    if (code_) VirtualFree(code_, 0, MEM_RELEASE);
}
}
