// SPDX-License-Identifier: GPL-2.0-only
// Applies the octr-mod mods of a folder to the user's own game folders, read-only, and
// reports each mod: octr_mods_control <mods folder> <update folder> <base folder>.
// Every mod is tried alone; the views are checked by reading each replaced entry back
// through the virtual archive. The mods' patches are planned on the eboot of the update
// folder, mapped as the runtime maps it (nothing is written to it), and ports from the
// other region are listed write by write.
#include "app0.hpp"
#include "flat_loader.hpp"
#include "octr_mods.hpp"
#include <iostream>

namespace fs = std::filesystem;
using namespace ctrport;

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: octr_mods_control <mods> <update> <base>\n";
        return 2;
    }
    const fs::path mods_dir = argv[1], patch = argv[2], base = argv[3];
    const auto sfo = guest_patches::read_sfo(patch / "sce_sys/param.sfo");
    const std::string title_id = sfo.at("TITLE_ID"), app_ver = sfo.at("APP_VER");
    ImageMetadata info;
    auto image = load_retail_image((patch / "eboot.bin").string().c_str(), false, &info);
    std::cout << "game " << title_id << " " << app_ver << ", eboot " << info.sha256.substr(0, 12) << "\n";
    std::uint64_t image_end = 0;
    for (const auto& span : info.spans) image_end = (std::max)(image_end, span.second);
    const auto functions = region_port::read_functions(image.get(), image_end);
    int failed = 0;
    for (const auto& entry : fs::directory_iterator(mods_dir)) {
        if (!entry.is_directory()) continue;
        const auto root = mods::content_root(entry.path());
        if (!mods::octr_mod(root)) continue;
        const mods::Mod mod{entry.path().filename().wstring(), root, true};
        try {
            const auto result = octr::build(
                {mod},
                [&](const std::string& relative) -> fs::path {
                    try {
                        return app0_read_path(base, patch, relative);
                    } catch (const std::exception&) {
                        return {};
                    }
                },
                title_id, app_ver);
            for (const auto& line : result.log) std::cout << "  " << line << "\n";
            if (!result.skipped.empty()) throw std::runtime_error(result.skipped.front().second);
            // Read every redirected entry back through the view.
            for (const auto& [key, view] : result.views)
                for (const auto& [at, descriptor] : view->patches) {
                    std::uint64_t offset = 0;
                    std::uint32_t size = 0, high = 0;
                    std::memcpy(&offset, descriptor.data(), 4);
                    std::memcpy(&high, descriptor.data() + 4, 4);
                    std::memcpy(&size, descriptor.data() + 8, 4);
                    offset |= std::uint64_t(high & 0xFF) << 32;
                    std::vector<std::uint8_t> bytes(size);
                    HANDLE file = CreateFileW(view->host.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                              0, nullptr);
                    const auto got = settings::menu_overlay::read(
                        *view, offset, bytes.data(), size, [&](std::uint64_t o, std::uint8_t* t, std::uint64_t n) {
                            OVERLAPPED ov{};
                            ov.Offset = DWORD(o);
                            ov.OffsetHigh = DWORD(o >> 32);
                            DWORD r = 0;
                            ReadFile(file, t, DWORD(n), &r, &ov);
                            return std::uint64_t(r);
                        });
                    CloseHandle(file);
                    if (got != size) throw std::runtime_error(key + ": short read of a replaced entry");
                }
            for (const auto& [m, what] : result.adapted) std::cout << "  adapted: " << what << "\n";
            for (const auto& [m, file] : result.patches)
                for (const auto& p : guest_patches::parse_patches(mods::read_text(file), file.filename().wstring())) {
                    std::vector<guest_patches::Write> writes;
                    if (guest_patches::mismatch(p, title_id, app_ver).empty()) {
                        writes = guest_patches::plan(p, image.get(), info.spans, true);
                        if (const auto bad = guest_patches::misplaced(writes, image.get(), functions); !bad.empty())
                            throw std::runtime_error(bad);
                        std::cout << "  patch " << p.name << ": planned as written\n";
                    } else {
                        std::string why, note;
                        writes = guest_patches::try_port(p, image.get(), info.spans, info.executable_spans, functions,
                                                         title_id, app_ver, why, note);
                        if (!why.empty()) throw std::runtime_error(why);
                        std::cout << "  patch " << p.name << ": ported, " << note << "\n";
                    }
                    for (const auto& w : writes) {
                        std::cout << "    write 0x" << std::hex << (w.offset + 0x400000) << std::dec << " "
                                  << w.bytes.size() << " bytes";
                        // where its calls and jumps land once written here
                        for (std::size_t at = 0; at + 5 <= w.bytes.size(); ++at)
                            if (w.bytes[at] == 0xE8 || w.bytes[at] == 0xE9) {
                                std::int32_t d;
                                std::memcpy(&d, &w.bytes[at + 1], 4);
                                const auto target =
                                    static_cast<std::uint64_t>(static_cast<std::int64_t>(w.offset + at + 5) + d);
                                if (target < info.spans.front().second)
                                    std::cout << " " << (w.bytes[at] == 0xE8 ? "call" : "jmp") << "@+" << at << "->0x"
                                              << std::hex << (target + 0x400000) << std::dec;
                            }
                        std::cout << "\n";
                    }
                }
            std::cout << "OK   " << mods::utf8(mod.name) << "\n";
        } catch (const std::exception& e) {
            ++failed;
            std::cout << "FAIL " << mods::utf8(mod.name) << ": " << e.what() << "\n";
        }
    }
    return failed ? 1 : 0;
}
