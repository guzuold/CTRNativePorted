// SPDX-License-Identifier: GPL-2.0-only
// Control for menu_assets.hpp: builds the PC Settings menu from the given retail archives
// and, with a reference folder made by tools/settings/assets (Python), checks that every
// resource and the rebuilt archive are byte-identical. Writes nothing.
// usage: menu_assets_control <juicedomain_permanent.pak> <update.pak> <update2.pak> [reference [--virtual]]
// --virtual also hashes the whole virtual update archives (menu_overlay.hpp) against the
// full copies the Python overlay wrote (several GB read).
#include "menu_overlay.hpp"
#include <chrono>
#include <cstdio>
#include <iostream>

using namespace ctrport::settings::menu_assets;

// SHA256 of a virtual archive streamed as the runtime serves it.
static std::string virtual_sha256(const ctrport::settings::menu_overlay::View& view) {
    std::ifstream in(view.host, std::ios::binary);
    BCRYPT_ALG_HANDLE alg{};
    BCRYPT_HASH_HANDLE hash{};
    std::array<unsigned char, 32> digest{};
    BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0);
    Bytes buffer(8u << 20);
    for (std::uint64_t at = 0; at < view.size();) {
        const auto got = ctrport::settings::menu_overlay::read(
            view, at, buffer.data(), buffer.size(), [&](std::uint64_t o, std::uint8_t* t, std::uint64_t n) {
                in.clear();
                in.seekg(std::streamoff(o));
                in.read(reinterpret_cast<char*>(t), std::streamsize(n));
                return std::uint64_t(in.gcount());
            });
        if (!got) throw std::runtime_error("short virtual read");
        BCryptHashData(hash, buffer.data(), ULONG(got), 0);
        at += got;
    }
    BCryptFinishHash(hash, digest.data(), 32, 0);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    std::string r;
    for (auto b : digest) {
        r += "0123456789abcdef"[b >> 4];
        r += "0123456789abcdef"[b & 15];
    }
    return r;
}

static Bytes read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("missing reference " + path.u8string());
    return Bytes(std::istreambuf_iterator<char>(in), {});
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr,
                     "usage: menu_assets_control <juicedomain_permanent.pak> <update.pak> <update2.pak> [reference]\n");
        return 2;
    }
    try {
        const auto start = std::chrono::steady_clock::now();
        const Generated g = generate(argv[1], argv[2], argv[3]);
        const auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        std::printf("generated %zu resources, %zu projects in %lld ms\n", g.files.size(), g.projects.size(),
                    static_cast<long long>(ms));
        std::printf("juicedomain_permanent.pak %zu bytes sha256=%s\n", g.juicedomain.size(),
                    sha256(g.juicedomain).c_str());
        for (const auto& [path, blob] : g.update)
            std::printf("update.pak  %s %s\n", path.c_str(), sha256(blob).c_str());
        for (const auto& [path, blob] : g.update2)
            std::printf("update2.pak %s %s\n", path.c_str(), sha256(blob).c_str());
        if (argc < 5) return 0;
        const fs::path reference = argv[4];
        size_t mismatches = 0;
        for (const auto& [path, blob] : g.files)
            if (read_file(reference / path) != blob) {
                std::printf("MISMATCH %s\n", path.c_str());
                ++mismatches;
            }
        if (read_file(reference / "archives/juicedomain_permanent.pak") != g.juicedomain) {
            std::printf("MISMATCH archives/juicedomain_permanent.pak\n");
            ++mismatches;
        }
        // The update replacements are recorded by hash in <layer>.overlay.json.
        for (const auto* layer : {&g.update, &g.update2}) {
            const std::string name = layer == &g.update ? "update.pak" : "update2.pak";
            const Bytes json = read_file(reference / (name + ".overlay.json"));
            const std::string text(json.begin(), json.end());
            for (const auto& [path, blob] : *layer)
                if (text.find("\"" + path + "\": \"" + sha256(blob) + "\"") == std::string::npos) {
                    std::printf("MISMATCH %s %s\n", name.c_str(), path.c_str());
                    ++mismatches;
                }
        }
        if (argc > 5 && std::string(argv[5]) == "--virtual") {
            for (int layer = 0; layer < 2; ++layer) {
                const auto view =
                    ctrport::settings::menu_overlay::append_entries(argv[2 + layer], layer ? g.update2 : g.update);
                const std::string name = layer ? "update2.pak" : "update.pak", got = virtual_sha256(*view);
                const Bytes json = read_file(reference / (name + ".overlay.json"));
                const bool same = std::string(json.begin(), json.end()).find("\"target_sha256\": \"" + got + "\"") !=
                                  std::string::npos;
                std::printf("virtual %s %llu bytes sha256=%s %s\n", name.c_str(),
                            static_cast<unsigned long long>(view->size()), got.c_str(),
                            same ? "identical" : "MISMATCH");
                mismatches += !same;
            }
        }
        std::printf(mismatches ? "MENU_ASSETS_MISMATCH %zu\n" : "MENU_ASSETS_IDENTICAL files=%zu\n",
                    mismatches ? mismatches : g.files.size() + g.update.size() + g.update2.size() + 1);
        return mismatches ? 1 : 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
