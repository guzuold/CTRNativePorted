// SPDX-License-Identifier: GPL-2.0-only
// Saves of the running game's title (EU CUSA14876, US CUSA13795) live in
// runtime/user/savedata/1/<title>/. When that folder holds no save yet, the runtime copies one in
// once, before the game starts:
//  1. the saves shadPS4 keeps for the same title (%APPDATA%/shadPS4/home/<user>/savedata/<title>,
//     or the older user/savedata/<user>/<title> layout); with several shadPS4 users, the one
//     whose saves changed last. hle/save_data.cpp reads their sce_sys/param.sfo;
//  2. otherwise, for a US eboot, the saves earlier versions of this runtime wrote under the EU
//     title CUSA14876 while running it.
// Nothing is moved or deleted, and a folder that already has saves is never touched.
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace ctrport::save_import {
namespace fs = std::filesystem;

// Save directories (with sce_sys/param.bin or sce_sys/param.sfo) directly under title.
inline std::size_t saves_in(const fs::path& title) {
    std::error_code ec;
    std::size_t count = 0;
    if (!fs::is_directory(title, ec)) return 0;
    for (const auto& entry : fs::directory_iterator(title, ec))
        if (entry.is_directory(ec) &&
            (fs::exists(entry.path() / "sce_sys/param.bin", ec) || fs::exists(entry.path() / "sce_sys/param.sfo", ec)))
            ++count;
    return count;
}

inline fs::file_time_type newest(const fs::path& folder) {
    std::error_code ec;
    fs::file_time_type result{};
    for (auto it = fs::recursive_directory_iterator(folder, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        const auto time = it->last_write_time(ec);
        if (!ec && time > result) result = time;
    }
    return result;
}

inline std::vector<fs::path> shadps4_sources(const std::string& title) {
    std::vector<fs::path> found;
    wchar_t appdata[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH);
    if (!length || length >= MAX_PATH) return found;
    const fs::path root = fs::path(appdata) / L"shadPS4";
    std::error_code ec;
    for (const auto& base : {root / L"home", root / L"user" / L"savedata", root / L"savedata"}) {
        if (!fs::is_directory(base, ec)) continue;
        for (const auto& user : fs::directory_iterator(base, ec)) {
            const auto candidate = base.filename() == L"home" ? user.path() / L"savedata" / title : user.path() / title;
            if (saves_in(candidate)) found.push_back(candidate);
        }
    }
    return found;
}

inline void run(const std::string& title) {
    const fs::path target = fs::path("runtime/user/savedata/1") / title;
    if (saves_in(target)) return;
    fs::path source;
    const char* origin = nullptr;
    const auto shadps4 = shadps4_sources(title);
    for (const auto& candidate : shadps4)
        if (source.empty() || newest(candidate) > newest(source)) source = candidate;
    if (!source.empty()) origin = "shadPS4";
    else if (title != "CUSA14876" && saves_in(fs::path("runtime/user/savedata/1/CUSA14876"))) {
        source = fs::path("runtime/user/savedata/1/CUSA14876");
        origin = "this runtime's earlier EU-titled folder";
    }
    if (source.empty()) return;
    std::error_code ec;
    fs::create_directories(target, ec);
    fs::copy(source, target, fs::copy_options::recursive | fs::copy_options::skip_existing, ec);
    if (ec) {
        std::fprintf(stderr, "saves: copying %s saves from %s failed: %s\n", title.c_str(), source.string().c_str(), ec.message().c_str());
    } else {
        std::fprintf(stderr, "saves: no %s save yet; copied %zu save folder(s) from %s (%s)\n", title.c_str(), saves_in(target), origin,
                     source.string().c_str());
    }
    std::fflush(stderr);
}
} // namespace ctrport::save_import
