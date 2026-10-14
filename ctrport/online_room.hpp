// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <string>
#include <stdexcept>
#pragma comment(lib, "bcrypt.lib")

// A room selector for the project server, not a console account credential.
inline std::wstring ctr_online_room_token(const std::string& password) {
    if (password.empty() || password.size() > 256)
        throw std::runtime_error("Room password must contain 1..256 UTF-8 bytes");
    const std::string input = std::string("ctr-room-v1\0",12) + password;
    BCRYPT_ALG_HANDLE provider = nullptr;
    if (BCryptOpenAlgorithmProvider(&provider, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        throw std::runtime_error("SHA256 provider unavailable");
    unsigned char digest[32]{};
    const auto status = BCryptHash(provider, nullptr, 0,
        reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())),
        static_cast<ULONG>(input.size()), digest, sizeof(digest));
    BCryptCloseAlgorithmProvider(provider, 0);
    if (status < 0) throw std::runtime_error("Room selector hashing failed");
    constexpr wchar_t hex[] = L"0123456789abcdef";
    std::wstring result;
    for (unsigned char byte : digest) { result += hex[byte >> 4]; result += hex[byte & 15]; }
    return result;
}
