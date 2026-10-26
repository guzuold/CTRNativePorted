// SPDX-License-Identifier: GPL-2.0-only
// Applies the octr-mod mods of a folder to the user's own game folders, read-only, and
// reports each mod: octr_mods_control <mods folder> <update folder> <base folder>.
// Every mod is tried alone; the views are checked by reading each replaced entry back
// through the virtual archive and comparing its SHA256 with the manifest's patched_sha256.
#include "app0.hpp"
#include "octr_mods.hpp"
#include <iostream>

namespace fs = std::filesystem;
using namespace ctrport;

int main(int argc, char** argv) {
    if (argc != 4) { std::cerr << "usage: octr_mods_control <mods> <update> <base>\n"; return 2; }
    const fs::path mods_dir = argv[1], patch = argv[2], base = argv[3];
    int failed = 0;
    for (const auto& entry : fs::directory_iterator(mods_dir)) {
        if (!entry.is_directory()) continue;
        const auto root = mods::content_root(entry.path());
        if (!mods::octr_mod(root)) continue;
        const mods::Mod mod{entry.path().filename().wstring(), root, true};
        try {
            const auto result = octr::build({mod}, [&](const std::string& relative) -> fs::path {
                try { return app0_read_path(base, patch, relative); } catch (const std::exception&) { return {}; }
            });
            // Read every redirected entry back through the view.
            for (const auto& [key, view] : result.views) {
                settings::menu_assets::Iga original(view->host, false);
                for (const auto& [at, descriptor] : view->patches) {
                    std::uint64_t offset = 0; std::uint32_t size = 0, high = 0;
                    std::memcpy(&offset, descriptor.data(), 4); std::memcpy(&high, descriptor.data() + 4, 4); std::memcpy(&size, descriptor.data() + 8, 4);
                    offset |= std::uint64_t(high & 0xFF) << 32;
                    std::vector<std::uint8_t> bytes(size);
                    HANDLE file = CreateFileW(view->host.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
                    const auto got = settings::menu_overlay::read(*view, offset, bytes.data(), size, [&](std::uint64_t o, std::uint8_t* t, std::uint64_t n) {
                        OVERLAPPED ov{}; ov.Offset = DWORD(o); ov.OffsetHigh = DWORD(o >> 32); DWORD r = 0;
                        ReadFile(file, t, DWORD(n), &r, &ov); return std::uint64_t(r);
                    });
                    CloseHandle(file);
                    if (got != size) throw std::runtime_error(key + ": short read of a replaced entry");
                    std::cout << "  " << key << " @" << at << ": " << size << " bytes, sha256 " << settings::menu_assets::sha256(bytes).substr(0, 16) << "\n";
                }
            }
            for (const auto& line : result.log) std::cout << "  " << line << "\n";
            std::cout << "OK   " << mods::utf8(mod.name) << "\n";
        } catch (const std::exception& e) {
            ++failed;
            std::cout << "FAIL " << mods::utf8(mod.name) << ": " << e.what() << "\n";
        }
    }
    return failed ? 1 : 0;
}
