// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
namespace ctrport::settings::ao {
struct Descriptor {
    std::uint64_t hash;
    std::size_t words;
    std::size_t operand_word;
    std::string_view sha256;
};
inline constexpr std::array<Descriptor, 4> descriptors{{
    {0x159945a4ULL, 704, 698, "cfb45acdb4e118da6076f4dd25ab6f35b2d9f319a182eb59c04124ae04aff634"},
    {0x4c779ee2ULL, 706, 700, "9e74b3da372951c0d3ece410bd31b86ae7bafd18916ca4d2caabe968eb88e305"},
    {0x693f51beULL, 710, 704, "4af3b639301eb0c3fd5012ec680144ce015f808670d590afa2d0e80081e1add7"},
    {0x8c8411feULL, 712, 706, "5bf68acc7abf86501cd5fa2f8fcc7568f6b9be07b7422242bdf267a0b7089393"},
}};
inline bool selected(std::string_view value) {
    if (value.empty() || value == "original") return false;
    if (value == "off") return true;
    throw std::runtime_error("AO: unsupported setting");
}
inline std::string digest(std::span<const std::uint32_t> code) {
    BCRYPT_ALG_HANDLE algorithm{};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        throw std::runtime_error("AO: SHA256 provider unavailable");
    std::array<unsigned char, 32> bytes{};
    const auto status =
        BCryptHash(algorithm, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<std::uint32_t*>(code.data())),
                   static_cast<ULONG>(code.size_bytes()), bytes.data(), static_cast<ULONG>(bytes.size()));
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (status < 0) throw std::runtime_error("AO: SHA256 failed");
    const char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (auto value : bytes) {
        result += hex[value >> 4];
        result += hex[value & 15];
    }
    return result;
}
// Called before translation; never modifies original guest storage. Empty means
// no override. The caller owns the returned vector until translation finishes.
inline std::vector<std::uint32_t> override_code(std::uint64_t hash, std::span<const std::uint32_t> original, bool off) {
    if (!off) return {};
    const Descriptor* found = nullptr;
    for (const auto& d : descriptors)
        if (d.hash == hash) {
            found = &d;
            break;
        }
    if (!found) return {};
    const auto& d = *found;
    if (original.size() != d.words || digest(original) != d.sha256)
        throw std::runtime_error("AO: recognized shader identity has changed");
    const auto at = d.operand_word;
    if (original[at - 1] != 0xd2000001u || original[at] != 0x01a9e501u || original[at + 1] != 0x5e020301u ||
        original[at + 2] != 0xbefe0430u || original[at + 3] != 0xf8001c0fu || original[at + 4] != 1 ||
        original[at + 5] != 0xbf810000u)
        throw std::runtime_error("AO: neutral-export contract mismatch");
    std::vector<std::uint32_t> result(original.begin(), original.end());
    // V_CNDMASK_B32 src0 becomes inline +1.0; src1 is already +1.0.
    // RG packing, BA register, EXEC restore and MRT export remain original.
    result[at] = (result[at] & ~0x1ffu) | 242u;
    return result;
}
} // namespace ctrport::settings::ao
