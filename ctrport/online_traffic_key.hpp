// SPDX-License-Identifier: GPL-2.0-only
// R8 (CTR_ONLINE=1): the Demonware client verifies every auth3 reply with RSA-PSS/SHA-256
// against a public key stored in the eboot (an RSA-2048 SubjectPublicKeyInfo, 0x126 bytes;
// the only one in the image). The project server cannot sign with that key, so the runtime
// replaces it in memory with the project server's public key; the original verification
// code is unchanged. The original bytes are checked by SHA-256, so no game data is stored in
// the repository. ctrport/GUEST_PATCHES.md, reports/online/r8_phase3_auth3.md.
#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#pragma comment(lib, "bcrypt.lib")

namespace ctrport {
inline std::string online_sha256_hex(const unsigned char* data, std::size_t size) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    unsigned char digest[32]{};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        throw std::runtime_error("SHA256 provider missing");
    const auto status = BCryptHash(algorithm, nullptr, 0, const_cast<unsigned char*>(data),
                                   static_cast<ULONG>(size), digest, 32);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status < 0) throw std::runtime_error("SHA256 failed");
    static const char hex[] = "0123456789abcdef";
    std::string text;
    for (const unsigned char b : digest) {
        text.push_back(hex[b >> 4]);
        text.push_back(hex[b & 15]);
    }
    return text;
}

// Applied when CTR_ONLINE=1. CTR_ONLINE_TRAFFIC_KEY names the project server's public key
// (DER SubjectPublicKeyInfo, RSA-2048, 0x126 bytes; play_gpu.py downloads it from the server).
inline void apply_online_traffic_key(unsigned char* image, const std::string& pin) {
    char online[2]{};
    if (GetEnvironmentVariableA("CTR_ONLINE", online, sizeof(online)) != 1 || online[0] != '1') return;
    constexpr std::size_t key_size = 0x126;
    constexpr char original_sha256[] =
        "356c4bc39a664dd9a14964a3a44258d898a0dbf54cfb10835cbdd63000c9a6b2";
    const std::uint64_t offset =
        pin == "39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5" ? 0x01becf40ULL
        : pin == "ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1" ? 0x01ade440ULL
        : 0ULL;
    if (!offset) throw std::runtime_error("Online traffic key has no binding for this eboot");
    char path[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableA("CTR_ONLINE_TRAFFIC_KEY", path, sizeof(path));
    if (!length || length >= sizeof(path))
        throw std::runtime_error("CTR_ONLINE=1 needs CTR_ONLINE_TRAFFIC_KEY (project server public key)");
    std::ifstream file(path, std::ios::binary);
    const std::vector<unsigned char> key{std::istreambuf_iterator<char>(file), {}};
    static const unsigned char spki_prefix[] = {0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a,
                                                0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01};
    if (key.size() != key_size || std::memcmp(key.data(), spki_prefix, sizeof(spki_prefix)))
        throw std::runtime_error("Project traffic key is not an RSA-2048 SubjectPublicKeyInfo");
    unsigned char* site = image + offset;
    if (online_sha256_hex(site, key_size) != original_sha256)
        throw std::runtime_error("Online traffic key site differs from the original key");
    DWORD old = 0;
    if (!VirtualProtect(site, key_size, PAGE_READWRITE, &old))
        throw std::runtime_error("Online traffic key unprotect failed");
    std::memcpy(site, key.data(), key_size);
    DWORD restored = 0;
    VirtualProtect(site, key_size, old, &restored);
    std::printf("{\"scope\":\"guest_online_traffic_key_applied\",\"target\":%llu,\"bytes\":%zu,\"key_sha256\":\"%s\"}\n",
                static_cast<unsigned long long>(offset), key_size,
                online_sha256_hex(key.data(), key.size()).c_str());
    std::fflush(stdout);
}
} // namespace ctrport
