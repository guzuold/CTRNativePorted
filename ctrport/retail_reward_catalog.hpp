// SPDX-License-Identifier: GPL-2.0-only
// Read-only observation of the user's retail reward table. No retail
// catalogue is shipped. Layout evidence: (development notes)
#pragma once
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <thread>

namespace ctrport {
namespace reward_catalog {
inline constexpr char supported_sha[] =
    "39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5";
template<class T> bool read(std::uintptr_t address, T& value) {
    SIZE_T done = 0;
    return address >= 0x10000 && ReadProcessMemory(GetCurrentProcess(),
        reinterpret_cast<const void*>(address), &value, sizeof(value), &done) && done == sizeof(value);
}
inline bool snapshot(std::uintptr_t base, std::string& result) {
    std::uintptr_t system = 0, list = 0, data = 0;
    std::uint32_t count = 0;
    if (!read(base + 0x2ad9c38, system) || !system || !read(system + 0x220, list) || !list ||
        !read(list + 0xc, count) || !count || count > 256 || !read(list + 0x20, data) || !data) return false;
    std::ostringstream out;
    out << "{\"schema\":1,\"eboot_sha256\":\"" << supported_sha << "\",\"tracks\":[";
    std::set<std::uint32_t> ids;
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uintptr_t entry = 0, handle = 0, zone = 0, name = 0;
        std::uint32_t flags = 0;
        std::array<std::int32_t, 4> amounts{};
        if (!read(data + i * 8, entry) || !entry || !read(entry + 0x10, handle) || !handle ||
            !read(handle, flags) || (flags & 0x30000000) || !read(handle + 0x28, zone) || !zone ||
            !read(zone + 0x88, name) || !name || !read(entry + 0x18, amounts)) return false;
        std::uint32_t id = 4;
        unsigned length = 0;
        for (; length < 128; ++length) {
            unsigned char c = 0;
            if (!read(name + length, c)) return false;
            if (!c) break;
            id = (id ^ c) * 0x1000193u;
        }
        if (!length || length == 128 || !ids.insert(id).second) return false;
        for (auto amount : amounts) if (amount < 0 || amount > 1000000) return false;
        if (i) out << ',';
        out << "{\"id\":" << id << ",\"ranks\":[" << amounts[0] << ',' << amounts[1]
            << ',' << amounts[2] << ',' << amounts[3] << "]}";
    }
    out << "]}\n";
    result = out.str();
    return true;
}
} // namespace reward_catalog
} // namespace ctrport
#include "retail_shop_catalog.hpp"
namespace ctrport::reward_catalog {
// The owner retains mapped guest images while the bounded observer is alive.
// Two equal snapshots are required; redirects/aliases are deferred, never resolved
// by calling guest code from a host thread.
template<class Owner> void start(unsigned char* image, const std::string& pin, Owner owner) {
    wchar_t path[32768]{};
    DWORD length = GetEnvironmentVariableW(L"CTR_REWARD_CATALOG", path, 32768);
    if (!length && pin != supported_sha) return;
    if (length >= 32768 || pin != supported_sha)
        throw std::runtime_error("Reward catalogue observation has no binding for this eboot/path");
    const auto destination = length ? std::filesystem::path(path) :
        std::filesystem::current_path() / "runtime/user/online/retail_reward_catalog.json";
    std::thread([base = reinterpret_cast<std::uintptr_t>(image), target = destination, owner] {
        (void)owner;
        try {
            std::string previous, current, previous_shop, current_shop;
            for (unsigned attempt = 0; attempt < 240; ++attempt) {
                if (snapshot(base, current) && shop_snapshot(base, current_shop)) {
                    if (current == previous && current_shop == previous_shop) {
                        std::filesystem::create_directories(target.parent_path());
                        auto temporary = target; temporary += L".tmp";
                        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
                        out << current; out.close();
                        if (!out || !MoveFileExW(temporary.c_str(), target.c_str(),
                                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                            throw std::runtime_error("Reward catalogue persistence failed");
                        const auto shop = target.parent_path() / "retail_shop_catalog.json";
                        auto shop_temporary = shop; shop_temporary += L".tmp";
                        std::ofstream shop_out(shop_temporary, std::ios::binary | std::ios::trunc);
                        shop_out << current_shop; shop_out.close();
                        if (!shop_out || !MoveFileExW(shop_temporary.c_str(), shop.c_str(),
                                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                            throw std::runtime_error("Shop catalogue persistence failed");
                        std::puts("{\"scope\":\"retail_reward_catalog\",\"saved\":true}");
                        std::fflush(stdout);
                        return;
                    }
                    previous = current;
                    previous_shop = current_shop;
                } else previous.clear();
                Sleep(500);
            }
            throw std::runtime_error("Reward catalogue not ready within 120 seconds");
        } catch (const std::exception&) {
            std::fputs("{\"scope\":\"retail_reward_catalog\",\"saved\":false}\n", stderr);
            std::fflush(stderr);
        }
    }).detach();
}
} // namespace reward_catalog
