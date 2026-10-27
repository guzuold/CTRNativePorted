// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <map>
#include <cstdio>
#include <stdexcept>
#include <cstdlib>
#include <string>
#pragma comment(lib, "bcrypt.lib")
namespace ctrport {
inline std::string prompt_file_hash(const std::filesystem::path& path) {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        throw std::runtime_error("Xbox icons: SHA256 provider failed");
    if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        throw std::runtime_error("Xbox icons: SHA256 init failed");
    }
    std::ifstream input(path, std::ios::binary);
    std::array<unsigned char, 65536> buffer{};
    std::array<unsigned char, 32> digest{};
    bool ok = bool(input);
    while (input) {
        input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        auto n = input.gcount();
        if (n && BCryptHashData(hash, buffer.data(), static_cast<ULONG>(n), 0) < 0) {
            ok = false;
            break;
        }
    }
    ok = ok && input.eof() && BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!ok) throw std::runtime_error("Xbox icons: file hash failed");
    std::string result;
    for (auto b : digest) {
        result += "0123456789abcdef"[b >> 4];
        result += "0123456789abcdef"[b & 15];
    }
    return result;
}
inline std::filesystem::path xbox_asset_overlay(const std::filesystem::path& original, const std::string& relative) {
    char enabled[4]{};
    if (GetEnvironmentVariableA("CTR_XBOX_PROMPTS", enabled, sizeof(enabled)) != 1 || enabled[0] != '1' ||
        (relative != "archives/permanent.pak" && relative != "archives/languagestartup.pak" &&
         relative != "archives/update.pak"))
        return original;
    static std::mutex mutex;
    static std::map<std::filesystem::path, std::filesystem::path> verified;
    std::lock_guard lock(mutex);
    if (auto it = verified.find(original); it != verified.end()) return it->second;
    const auto source_hash = prompt_file_hash(original);
    const auto root = std::filesystem::canonical("runtime/user");
    const auto candidate = root / "mods/xbox-default" / source_hash / original.filename();
    if (!std::filesystem::is_regular_file(candidate))
        throw std::runtime_error(
            "Xbox icons not prepared for this retail archive; run tools/input_prompts/prepare_xbox.py or select PS4 icons");
    const auto actual = std::filesystem::canonical(candidate);
    const auto within = actual.lexically_relative(root);
    for (const auto& part : within)
        if (part == "..") throw std::runtime_error("Xbox overlay escapes runtime/user");
    std::ifstream checksum(candidate.string() + ".sha256");
    std::string expected;
    checksum >> expected;
    if (expected.size() != 64 || prompt_file_hash(actual) != expected)
        throw std::runtime_error("Xbox icons archive checksum mismatch");
    std::fprintf(stderr, "ctrport: Xbox Default icons enabled; source archive SHA256=%s\n", source_hash.c_str());
    verified.emplace(original, actual);
    return actual;
}
}
