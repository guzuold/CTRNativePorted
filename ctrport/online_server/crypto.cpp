// SPDX-License-Identifier: GPL-2.0-or-later
#include "crypto.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <fstream>
#include <iterator>
#include <stdexcept>
#pragma comment(lib, "bcrypt.lib")

namespace ctr_online {
namespace {
void check(NTSTATUS status, const char* what) {
    if (status < 0) throw std::runtime_error(std::string("CNG failure: ") + what);
}
struct Provider {
    BCRYPT_ALG_HANDLE handle = nullptr;
    Provider(const wchar_t* algorithm, ULONG flags = 0) {
        check(BCryptOpenAlgorithmProvider(&handle, algorithm, nullptr, flags), "open provider");
    }
    ~Provider() { if (handle) BCryptCloseAlgorithmProvider(handle, 0); }
};
Bytes hash(const wchar_t* algorithm, const Bytes* key, const Bytes& data, ULONG size) {
    Provider p(algorithm, key ? BCRYPT_ALG_HANDLE_HMAC_FLAG : 0);
    Bytes out(size);
    check(BCryptHash(p.handle, key ? const_cast<PUCHAR>(key->data()) : nullptr,
                     key ? static_cast<ULONG>(key->size()) : 0,
                     const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), out.data(), size),
          "hash");
    return out;
}
Bytes aes(const Bytes& key, const Bytes& iv, const Bytes& data, bool encrypt) {
    if (key.size() != 16 || iv.size() != 16 || data.size() % 16) throw std::invalid_argument("AES-128-CBC sizes");
    Provider p(BCRYPT_AES_ALGORITHM);
    check(BCryptSetProperty(p.handle, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_CBC)),
                            sizeof(BCRYPT_CHAIN_MODE_CBC), 0), "CBC mode");
    BCRYPT_KEY_HANDLE handle = nullptr;
    check(BCryptGenerateSymmetricKey(p.handle, &handle, nullptr, 0, const_cast<PUCHAR>(key.data()), 16, 0), "AES key");
    Bytes vector = iv, out(data.size());
    ULONG written = 0;
    const NTSTATUS status = encrypt
        ? BCryptEncrypt(handle, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), nullptr,
                        vector.data(), 16, out.data(), static_cast<ULONG>(out.size()), &written, 0)
        : BCryptDecrypt(handle, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), nullptr,
                        vector.data(), 16, out.data(), static_cast<ULONG>(out.size()), &written, 0);
    BCryptDestroyKey(handle);
    check(status, "AES");
    out.resize(written);
    return out;
}
// DER helpers for the SubjectPublicKeyInfo.
Bytes der_length(std::size_t n) {
    if (n < 0x80) return {static_cast<unsigned char>(n)};
    Bytes raw;
    while (n) { raw.insert(raw.begin(), static_cast<unsigned char>(n & 0xff)); n >>= 8; }
    raw.insert(raw.begin(), static_cast<unsigned char>(0x80 | raw.size()));
    return raw;
}
Bytes der(unsigned char tag, const Bytes& content) {
    Bytes out{tag};
    const Bytes length = der_length(content.size());
    out.insert(out.end(), length.begin(), length.end());
    out.insert(out.end(), content.begin(), content.end());
    return out;
}
Bytes der_integer(Bytes big_endian) {
    while (big_endian.size() > 1 && big_endian[0] == 0) big_endian.erase(big_endian.begin());
    if (big_endian[0] & 0x80) big_endian.insert(big_endian.begin(), 0);
    return der(0x02, big_endian);
}
} // namespace

Bytes random_bytes(std::size_t count) {
    Bytes out(count);
    if (count) check(BCryptGenRandom(nullptr, out.data(), static_cast<ULONG>(count), BCRYPT_USE_SYSTEM_PREFERRED_RNG), "random");
    return out;
}
std::uint64_t random_u64() {
    const Bytes b = random_bytes(8);
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= static_cast<std::uint64_t>(b[i]) << (8 * i);
    return v;
}
Bytes sha1(const Bytes& data) { return hash(BCRYPT_SHA1_ALGORITHM, nullptr, data, 20); }
Bytes sha256(const Bytes& data) { return hash(BCRYPT_SHA256_ALGORITHM, nullptr, data, 32); }
Bytes hmac_sha1(const Bytes& key, const Bytes& data) { return hash(BCRYPT_SHA1_ALGORITHM, &key, data, 20); }
Bytes hkdf_expand_sha1(const Bytes& prk, const std::string& info, std::size_t length) {
    Bytes out, block;
    for (unsigned char counter = 1; out.size() < length; ++counter) {
        Bytes input = block;
        input.insert(input.end(), info.begin(), info.end());
        input.push_back(counter);
        block = hmac_sha1(prk, input);
        out.insert(out.end(), block.begin(), block.end());
    }
    out.resize(length);
    return out;
}
Bytes aes128_cbc_encrypt(const Bytes& key, const Bytes& iv, const Bytes& data) { return aes(key, iv, data, true); }
Bytes aes128_cbc_decrypt(const Bytes& key, const Bytes& iv, const Bytes& data) { return aes(key, iv, data, false); }
bool constant_time_equal(const unsigned char* a, const unsigned char* b, std::size_t size) {
    unsigned char diff = 0;
    for (std::size_t i = 0; i < size; ++i) diff |= a[i] ^ b[i];
    return diff == 0;
}

std::string base64_encode(const Bytes& data) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (std::size_t i = 0; i < data.size(); i += 3) {
        const std::uint32_t chunk = data[i] << 16 | (i + 1 < data.size() ? data[i + 1] << 8 : 0) |
                                    (i + 2 < data.size() ? data[i + 2] : 0);
        out += table[chunk >> 18 & 63];
        out += table[chunk >> 12 & 63];
        out += i + 1 < data.size() ? table[chunk >> 6 & 63] : '=';
        out += i + 2 < data.size() ? table[chunk & 63] : '=';
    }
    return out;
}
Bytes base64_decode(const std::string& text) {
    // Python base64.b64decode(validate=True): only alphabet and '=' padding, length % 4 == 0.
    if (text.size() % 4) throw std::invalid_argument("base64 length");
    Bytes out;
    std::uint32_t buffer = 0;
    int bits = 0;
    std::size_t padding = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        int v;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
        else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '+') v = 62;
        else if (c == '/') v = 63;
        else if (c == '=' && i >= text.size() - 2) { ++padding; continue; }
        else throw std::invalid_argument("base64 character");
        if (padding) throw std::invalid_argument("base64 data after padding");
        buffer = (buffer << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back(static_cast<unsigned char>(buffer >> bits)); }
    }
    return out;
}
std::string hex(const Bytes& data) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    for (const unsigned char b : data) { out += digits[b >> 4]; out += digits[b & 15]; }
    return out;
}
Bytes from_hex(const std::string& text) {
    if (text.size() % 2) throw std::invalid_argument("hex length");
    Bytes out;
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        throw std::invalid_argument("hex digit");
    };
    for (std::size_t i = 0; i < text.size(); i += 2) out.push_back(static_cast<unsigned char>(nibble(text[i]) << 4 | nibble(text[i + 1])));
    return out;
}
Bytes bytes_of(const std::string& text) { return Bytes(text.begin(), text.end()); }

RsaKey::RsaKey(const std::filesystem::path& path) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    check(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_RSA_ALGORITHM, nullptr, 0), "RSA provider");
    algorithm_ = algorithm;
    BCRYPT_KEY_HANDLE key = nullptr;
    std::ifstream in(path, std::ios::binary);
    Bytes blob{std::istreambuf_iterator<char>(in), {}};
    if (!blob.empty()) {
        check(BCryptImportKeyPair(algorithm, nullptr, BCRYPT_RSAFULLPRIVATE_BLOB, &key, blob.data(),
                                  static_cast<ULONG>(blob.size()), 0), "RSA import");
    } else {
        check(BCryptGenerateKeyPair(algorithm, &key, 2048, 0), "RSA generate");
        check(BCryptFinalizeKeyPair(key, 0), "RSA finalize");
        ULONG size = 0;
        check(BCryptExportKey(key, nullptr, BCRYPT_RSAFULLPRIVATE_BLOB, nullptr, 0, &size, 0), "RSA export size");
        blob.resize(size);
        check(BCryptExportKey(key, nullptr, BCRYPT_RSAFULLPRIVATE_BLOB, blob.data(), size, &size, 0), "RSA export");
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(blob.data()), size);
    }
    key_ = key;
    // Public part -> SubjectPublicKeyInfo DER.
    ULONG size = 0;
    check(BCryptExportKey(key, nullptr, BCRYPT_RSAPUBLIC_BLOB, nullptr, 0, &size, 0), "RSA public size");
    Bytes pub(size);
    check(BCryptExportKey(key, nullptr, BCRYPT_RSAPUBLIC_BLOB, pub.data(), size, &size, 0), "RSA public");
    const auto* header = reinterpret_cast<const BCRYPT_RSAKEY_BLOB*>(pub.data());
    const unsigned char* p = pub.data() + sizeof(BCRYPT_RSAKEY_BLOB);
    const Bytes exponent(p, p + header->cbPublicExp);
    const Bytes modulus(p + header->cbPublicExp, p + header->cbPublicExp + header->cbModulus);
    Bytes rsa_public = der_integer(modulus);
    const Bytes e = der_integer(exponent);
    rsa_public.insert(rsa_public.end(), e.begin(), e.end());
    Bytes bit_string{0};
    const Bytes sequence = der(0x30, rsa_public);
    bit_string.insert(bit_string.end(), sequence.begin(), sequence.end());
    const Bytes algorithm_id = der(0x30, {0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00});
    Bytes info = algorithm_id;
    const Bytes bits = der(0x03, bit_string);
    info.insert(info.end(), bits.begin(), bits.end());
    spki_ = der(0x30, info);
}
RsaKey::~RsaKey() {
    if (key_) BCryptDestroyKey(static_cast<BCRYPT_KEY_HANDLE>(key_));
    if (algorithm_) BCryptCloseAlgorithmProvider(static_cast<BCRYPT_ALG_HANDLE>(algorithm_), 0);
}
Bytes RsaKey::sign_pss_sha256(const Bytes& message) const {
    const Bytes digest = sha256(message);
    BCRYPT_PSS_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM, 0};
    ULONG size = 0;
    check(BCryptSignHash(static_cast<BCRYPT_KEY_HANDLE>(key_), &padding, const_cast<PUCHAR>(digest.data()), 32,
                         nullptr, 0, &size, BCRYPT_PAD_PSS), "PSS size");
    Bytes signature(size);
    check(BCryptSignHash(static_cast<BCRYPT_KEY_HANDLE>(key_), &padding, const_cast<PUCHAR>(digest.data()), 32,
                         signature.data(), size, &size, BCRYPT_PAD_PSS), "PSS sign");
    signature.resize(size);
    return signature;
}
} // namespace ctr_online
