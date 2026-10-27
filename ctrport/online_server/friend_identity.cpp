// SPDX-License-Identifier: GPL-2.0-or-later
#include "friend_identity.hpp"
#include "crypto.hpp"
#include "json.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>

#pragma comment(lib, "bcrypt.lib")

namespace ctr_online {
namespace {

// Standard IEEE 802.3 CRC32
std::uint32_t crc32(const unsigned char* data, std::size_t len) {
    std::uint32_t crc = 0xFFFFFFFF;
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(std::int32_t)(crc & 1)));
        }
    }
    return ~crc;
}

// Crockford Base32 alphabet: 0-9, A-Z without I, L, O, U
const char kCrockfordChars[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

std::string crockford_encode(const unsigned char* data, std::size_t len) {
    std::string out;
    std::size_t bits = 0;
    std::uint32_t buffer = 0;
    for (std::size_t i = 0; i < len; ++i) {
        buffer = (buffer << 8) | data[i];
        bits += 8;
        while (bits >= 5) {
            bits -= 5;
            out += kCrockfordChars[(buffer >> bits) & 0x1F];
        }
    }
    if (bits > 0) {
        out += kCrockfordChars[(buffer << (5 - bits)) & 0x1F];
    }
    return out;
}

bool crockford_decode(const std::string& text, Bytes& out) {
    out.clear();
    std::size_t bits = 0;
    std::uint32_t buffer = 0;
    for (char c : text) {
        if (c == '-' || c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        int val = -1;
        if (c >= '0' && c <= '9')
            val = c - '0';
        else if (c >= 'A' && c <= 'Z') {
            if (c == 'I' || c == 'L')
                val = 1;
            else if (c == 'O')
                val = 0;
            else {
                const char* p = std::strchr(kCrockfordChars, c);
                if (p) val = static_cast<int>(p - kCrockfordChars);
            }
        } else if (c >= 'a' && c <= 'z') {
            char u = static_cast<char>(c - 'a' + 'A');
            if (u == 'I' || u == 'L')
                val = 1;
            else if (u == 'O')
                val = 0;
            else {
                const char* p = std::strchr(kCrockfordChars, u);
                if (p) val = static_cast<int>(p - kCrockfordChars);
            }
        }
        if (val < 0) return false;
        buffer = (buffer << 5) | static_cast<std::uint32_t>(val);
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<unsigned char>((buffer >> bits) & 0xFF));
        }
    }
    if (bits > 0 && (buffer & ((1 << bits) - 1)) != 0) {
        return false;
    }
    return true;
}

} // namespace

const char* code_parse_error_string(CodeParseError err) {
    switch (err) {
    case CodeParseError::Ok: return "Ok";
    case CodeParseError::Empty: return "Friend code is empty";
    case CodeParseError::InvalidPrefix: return "Friend code prefix must start with CTR1";
    case CodeParseError::InvalidCharacters: return "Friend code contains invalid characters";
    case CodeParseError::InvalidLength: return "Friend code has incorrect length";
    case CodeParseError::UnsupportedVersion: return "Friend code version is not supported";
    case CodeParseError::ChecksumMismatch: return "Friend code checksum failed (mistyped or corrupted)";
    default: return "Unknown friend code error";
    }
}

std::string format_friend_code(std::uint8_t version, const Bytes& public_key) {
    if (public_key.size() != 64) {
        throw std::invalid_argument("Public key must be 64 bytes (P-256 X, Y)");
    }
    Bytes payload;
    payload.reserve(1 + 64 + 4);
    payload.push_back(version);
    payload.insert(payload.end(), public_key.begin(), public_key.end());
    const std::uint32_t check = crc32(payload.data(), payload.size());
    payload.push_back(static_cast<unsigned char>((check >> 24) & 0xFF));
    payload.push_back(static_cast<unsigned char>((check >> 16) & 0xFF));
    payload.push_back(static_cast<unsigned char>((check >> 8) & 0xFF));
    payload.push_back(static_cast<unsigned char>(check & 0xFF));

    const std::string raw_b32 = crockford_encode(payload.data(), payload.size());
    std::string formatted = "CTR1";
    for (std::size_t i = 0; i < raw_b32.size(); ++i) {
        if (i % 5 == 0) formatted += '-';
        formatted += raw_b32[i];
    }
    return formatted;
}

CodeParseError parse_friend_code(const std::string& code, FriendCodeInfo& out) {
    if (code.empty()) return CodeParseError::Empty;

    std::string clean;
    clean.reserve(code.size());
    for (char c : code) {
        if (c != '-' && c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            clean += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
    }
    if (clean.size() < 4 || clean.compare(0, 4, "CTR1") != 0) {
        return CodeParseError::InvalidPrefix;
    }
    const std::string b32_part = clean.substr(4);
    if (b32_part.size() != 111) {
        return CodeParseError::InvalidLength;
    }
    Bytes decoded;
    if (!crockford_decode(b32_part, decoded)) {
        return CodeParseError::InvalidCharacters;
    }
    if (decoded.size() != 1 + 64 + 4) {
        return CodeParseError::InvalidLength;
    }
    const std::uint8_t version = decoded[0];
    if (version != 1) {
        return CodeParseError::UnsupportedVersion;
    }
    const std::uint32_t expected_crc =
        (static_cast<std::uint32_t>(decoded[65]) << 24) | (static_cast<std::uint32_t>(decoded[66]) << 16) |
        (static_cast<std::uint32_t>(decoded[67]) << 8) | static_cast<std::uint32_t>(decoded[68]);
    const std::uint32_t calculated_crc = crc32(decoded.data(), 65);
    if (expected_crc != calculated_crc) {
        return CodeParseError::ChecksumMismatch;
    }

    out.version = version;
    out.public_key.assign(decoded.begin() + 1, decoded.begin() + 65);
    out.code = format_friend_code(version, out.public_key);
    return CodeParseError::Ok;
}

Bytes friend_dht_info_hash(const Bytes& public_key_64) {
    Bytes input = bytes_of("CTR_DHT_DISCOVERY_v1:");
    input.insert(input.end(), public_key_64.begin(), public_key_64.end());
    return sha1(input);
}

HostIdentity::HostIdentity() = default;

HostIdentity::~HostIdentity() {
    if (key_) BCryptDestroyKey(reinterpret_cast<BCRYPT_KEY_HANDLE>(key_));
    if (algorithm_) BCryptCloseAlgorithmProvider(reinterpret_cast<BCRYPT_ALG_HANDLE>(algorithm_), 0);
}

HostIdentity::HostIdentity(HostIdentity&& o) noexcept
    : algorithm_(o.algorithm_), key_(o.key_), public_key_(std::move(o.public_key_)),
      private_blob_(std::move(o.private_blob_)), friend_code_(std::move(o.friend_code_)),
      dht_info_hash_(std::move(o.dht_info_hash_)) {
    o.algorithm_ = nullptr;
    o.key_ = nullptr;
}

HostIdentity& HostIdentity::operator=(HostIdentity&& o) noexcept {
    if (this != &o) {
        if (key_) BCryptDestroyKey(reinterpret_cast<BCRYPT_KEY_HANDLE>(key_));
        if (algorithm_) BCryptCloseAlgorithmProvider(reinterpret_cast<BCRYPT_ALG_HANDLE>(algorithm_), 0);
        algorithm_ = o.algorithm_;
        key_ = o.key_;
        public_key_ = std::move(o.public_key_);
        private_blob_ = std::move(o.private_blob_);
        friend_code_ = std::move(o.friend_code_);
        dht_info_hash_ = std::move(o.dht_info_hash_);
        o.algorithm_ = nullptr;
        o.key_ = nullptr;
    }
    return *this;
}

HostIdentity HostIdentity::load_or_create(const std::filesystem::path& file_path) {
    HostIdentity ident;
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) != 0) {
        throw std::runtime_error("BCryptOpenAlgorithmProvider ECDSA_P256 failed");
    }
    ident.algorithm_ = alg;

    bool loaded = false;
    if (std::filesystem::exists(file_path)) {
        try {
            std::ifstream file(file_path);
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            Json root = parse_json(content);
            const std::string priv_hex = root.at("private_blob_hex").text;
            Bytes blob = from_hex(priv_hex);
            BCRYPT_KEY_HANDLE key = nullptr;
            if (BCryptImportKeyPair(alg, nullptr, BCRYPT_ECCPRIVATE_BLOB, &key, blob.data(),
                                    static_cast<ULONG>(blob.size()), 0) == 0) {
                ident.key_ = key;
                ident.private_blob_ = blob;
                loaded = true;
            }
        } catch (...) {
            loaded = false;
        }
    }

    if (!loaded && std::filesystem::exists(file_path))
        throw std::runtime_error("Existing host identity is unreadable; refusing to replace friend code");

    if (!loaded) {
        BCRYPT_KEY_HANDLE key = nullptr;
        if (BCryptGenerateKeyPair(alg, &key, 256, 0) != 0 || BCryptFinalizeKeyPair(key, 0) != 0) {
            throw std::runtime_error("BCryptGenerateKeyPair ECDSA_P256 failed");
        }
        ident.key_ = key;

        DWORD priv_len = 0;
        BCryptExportKey(key, nullptr, BCRYPT_ECCPRIVATE_BLOB, nullptr, 0, &priv_len, 0);
        ident.private_blob_.resize(priv_len);
        if (BCryptExportKey(key, nullptr, BCRYPT_ECCPRIVATE_BLOB, ident.private_blob_.data(), priv_len, &priv_len, 0) !=
            0) {
            throw std::runtime_error("BCryptExportKey ECCPRIVATE_BLOB failed");
        }
    }

    // Export public key blob to extract X, Y
    DWORD pub_len = 0;
    BCryptExportKey(reinterpret_cast<BCRYPT_KEY_HANDLE>(ident.key_), nullptr, BCRYPT_ECCPUBLIC_BLOB, nullptr, 0,
                    &pub_len, 0);
    Bytes pub_blob(pub_len);
    if (BCryptExportKey(reinterpret_cast<BCRYPT_KEY_HANDLE>(ident.key_), nullptr, BCRYPT_ECCPUBLIC_BLOB,
                        pub_blob.data(), pub_len, &pub_len, 0) != 0 ||
        pub_len < 72) {
        throw std::runtime_error("BCryptExportKey ECCPUBLIC_BLOB failed");
    }
    // ECCPUBLIC_BLOB format: 8-byte header (dwMagic, cbKey=32), followed by 32 bytes X and 32 bytes Y
    ident.public_key_.assign(pub_blob.begin() + 8, pub_blob.begin() + 8 + 64);
    ident.friend_code_ = format_friend_code(1, ident.public_key_);
    ident.dht_info_hash_ = friend_dht_info_hash(ident.public_key_);

    // Save if not loaded or newly generated
    if (!loaded) {
        std::error_code ec;
        std::filesystem::create_directories(file_path.parent_path(), ec);
        Json root = Json::object();
        root.set("version", Json::of(1));
        root.set("friend_code", Json::of(ident.friend_code_));
        root.set("public_key_hex", Json::of(hex(ident.public_key_)));
        root.set("private_blob_hex", Json::of(hex(ident.private_blob_)));
        std::ofstream out(file_path, std::ios::trunc);
        out << dump_json_spaced(root) << '\n';
        out.close();
        if (!out) throw std::runtime_error("Unable to persist host identity");
    }

    return ident;
}

Bytes HostIdentity::sign_challenge(const std::string& nonce_hex, const Bytes& traffic_key_spki,
                                   std::uint64_t timestamp) const {
    if (!key_) throw std::runtime_error("No host identity key");
    const std::string message =
        "CTR-FRIEND-AUTH-V1:" + nonce_hex + ":" + hex(sha256(traffic_key_spki)) + ":" + std::to_string(timestamp);
    const Bytes hash = sha256(bytes_of(message));

    DWORD sig_len = 0;
    BCryptSignHash(reinterpret_cast<BCRYPT_KEY_HANDLE>(key_), nullptr, const_cast<unsigned char*>(hash.data()),
                   static_cast<ULONG>(hash.size()), nullptr, 0, &sig_len, 0);
    Bytes signature(sig_len);
    if (BCryptSignHash(reinterpret_cast<BCRYPT_KEY_HANDLE>(key_), nullptr, const_cast<unsigned char*>(hash.data()),
                       static_cast<ULONG>(hash.size()), signature.data(), sig_len, &sig_len, 0) != 0) {
        throw std::runtime_error("BCryptSignHash ECDSA failed");
    }
    return signature;
}

bool verify_friend_response(const Bytes& public_key_64, const std::string& nonce_hex, const Bytes& traffic_key_spki,
                            std::uint64_t timestamp, const Bytes& signature_64) {
    if (public_key_64.size() != 64 || signature_64.size() != 64) return false;

    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) != 0) return false;

    // Construct BCRYPT_ECCKEY_BLOB
    // Magic: BCRYPT_ECDSA_PUBLIC_P256_MAGIC (0x31534345)
    // cbKey: 32
    Bytes blob(8 + 64);
    const std::uint32_t magic = 0x31534345;
    const std::uint32_t cbKey = 32;
    std::memcpy(blob.data(), &magic, 4);
    std::memcpy(blob.data() + 4, &cbKey, 4);
    std::memcpy(blob.data() + 8, public_key_64.data(), 64);

    BCRYPT_KEY_HANDLE pub_key = nullptr;
    if (BCryptImportKeyPair(alg, nullptr, BCRYPT_ECCPUBLIC_BLOB, &pub_key, blob.data(), static_cast<ULONG>(blob.size()),
                            0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }

    const std::string message =
        "CTR-FRIEND-AUTH-V1:" + nonce_hex + ":" + hex(sha256(traffic_key_spki)) + ":" + std::to_string(timestamp);
    const Bytes hash = sha256(bytes_of(message));

    const NTSTATUS status = BCryptVerifySignature(
        pub_key, nullptr, const_cast<unsigned char*>(hash.data()), static_cast<ULONG>(hash.size()),
        const_cast<unsigned char*>(signature_64.data()), static_cast<ULONG>(signature_64.size()), 0);

    BCryptDestroyKey(pub_key);
    BCryptCloseAlgorithmProvider(alg, 0);
    return status == 0;
}

} // namespace ctr_online
