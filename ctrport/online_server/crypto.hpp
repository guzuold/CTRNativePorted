// SPDX-License-Identifier: GPL-2.0-or-later
// Cryptography of the project server over Windows CNG (bcrypt.dll): no own primitives.
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ctr_online {
using Bytes = std::vector<unsigned char>;

Bytes random_bytes(std::size_t count);
std::uint64_t random_u64();
Bytes sha1(const Bytes& data);
Bytes sha256(const Bytes& data);
Bytes hmac_sha1(const Bytes& key, const Bytes& data);
Bytes hkdf_expand_sha1(const Bytes& prk, const std::string& info, std::size_t length);
// AES-128-CBC without padding (lengths are multiples of 16).
Bytes aes128_cbc_encrypt(const Bytes& key, const Bytes& iv, const Bytes& data);
Bytes aes128_cbc_decrypt(const Bytes& key, const Bytes& iv, const Bytes& data);
bool constant_time_equal(const unsigned char* a, const unsigned char* b, std::size_t size);

std::string base64_encode(const Bytes& data);
Bytes base64_decode(const std::string& text); // strict (validate=True); throws
std::string hex(const Bytes& data);
Bytes from_hex(const std::string& text); // throws
Bytes bytes_of(const std::string& text);

// RSA-2048 server key, kept as a CNG full private blob in `path` (created when missing).
class RsaKey {
public:
    explicit RsaKey(const std::filesystem::path& path);
    ~RsaKey();
    RsaKey(const RsaKey&) = delete;
    RsaKey& operator=(const RsaKey&) = delete;
    // X.509 SubjectPublicKeyInfo DER (0x126 bytes for RSA-2048).
    const Bytes& spki() const { return spki_; }
    // RSASSA-PSS, SHA-256, MGF1-SHA-256, salt length 0 (the game's verifier).
    Bytes sign_pss_sha256(const Bytes& message) const;

private:
    void* algorithm_ = nullptr;
    void* key_ = nullptr;
    Bytes spki_;
};
} // namespace ctr_online
