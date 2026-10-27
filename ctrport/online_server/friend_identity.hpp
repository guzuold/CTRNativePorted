// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ctr_online {

using Bytes = std::vector<unsigned char>;

struct FriendCodeInfo {
    std::uint8_t version = 0;
    Bytes public_key; // 64 bytes (X, Y)
    std::string code; // Canonical Crockford Base32 with hyphens: CTR1-XXXXX-XXXXX-...
};

enum class CodeParseError {
    Ok = 0,
    Empty,
    InvalidPrefix,
    InvalidCharacters,
    InvalidLength,
    UnsupportedVersion,
    ChecksumMismatch
};

const char* code_parse_error_string(CodeParseError err);

// Parses a friend code string, returns parsed info or error.
CodeParseError parse_friend_code(const std::string& code, FriendCodeInfo& out);

// Formats version (1) + 64-byte public key into canonical friend code string.
std::string format_friend_code(std::uint8_t version, const Bytes& public_key);

// Derives the 20-byte DHT infohash with domain separation:
// SHA-1("CTR_DHT_DISCOVERY_v1:" || public_key_64)
Bytes friend_dht_info_hash(const Bytes& public_key_64);

// Manages persistent host ECDSA P-256 identity keypair.
class HostIdentity {
public:
    HostIdentity();
    ~HostIdentity();
    HostIdentity(const HostIdentity&) = delete;
    HostIdentity& operator=(const HostIdentity&) = delete;
    HostIdentity(HostIdentity&&) noexcept;
    HostIdentity& operator=(HostIdentity&&) noexcept;

    // Loads identity from file or generates a new one and saves it.
    static HostIdentity load_or_create(const std::filesystem::path& file_path);

    // 64-byte uncompressed public key (X, Y)
    const Bytes& public_key() const { return public_key_; }
    // Canonical friend code (CTR1-...)
    const std::string& friend_code() const { return friend_code_; }
    // 20-byte DHT infohash
    const Bytes& dht_info_hash() const { return dht_info_hash_; }

    // Signs the authentication challenge for a client.
    // Message signed: "CTR-FRIEND-AUTH-V1:" + nonce_hex + ":" + hex(sha256(traffic_key_spki)) + ":" + std::to_string(timestamp)
    Bytes sign_challenge(const std::string& nonce_hex, const Bytes& traffic_key_spki, std::uint64_t timestamp) const;

private:
    void* algorithm_ = nullptr;
    void* key_ = nullptr;
    Bytes public_key_;   // 64 bytes (X, Y)
    Bytes private_blob_; // CNG export blob
    std::string friend_code_;
    Bytes dht_info_hash_; // 20 bytes
};

// Verifies an authentication response using a friend's public key (64 bytes).
bool verify_friend_response(const Bytes& public_key_64, const std::string& nonce_hex, const Bytes& traffic_key_spki,
                            std::uint64_t timestamp, const Bytes& signature_64);

} // namespace ctr_online
