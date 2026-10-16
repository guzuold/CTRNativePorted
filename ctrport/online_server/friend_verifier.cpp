// SPDX-License-Identifier: GPL-2.0-or-later
#include "friend_verifier.hpp"
#include "crypto.hpp"
#include "json.hpp"
#include "launcher_online.hpp"
#include <chrono>
#include <cmath>

namespace ctr_online {

const char* verify_status_string(VerifyStatus status) {
    switch (status) {
    case VerifyStatus::Verified: return "Server verified";
    case VerifyStatus::NoResponse: return "No response from server";
    case VerifyStatus::InvalidIdentity: return "Identity mismatch or invalid signature";
    case VerifyStatus::MalformedResponse: return "Malformed or unexpected response";
    case VerifyStatus::InvalidTrafficKey: return "Traffic key mismatch or corrupted";
    default: return "Unknown verification status";
    }
}

VerifyResult verify_candidate_endpoint(const std::string& candidate_endpoint,
                                       const FriendCodeInfo& friend_info,
                                       int timeout_ms) {
    VerifyResult res;
    std::string ip = candidate_endpoint;
    std::uint16_t port = 8780;
    const std::size_t colon = candidate_endpoint.rfind(':');
    if (colon != std::string::npos) {
        ip = candidate_endpoint.substr(0, colon);
        const int p = std::atoi(candidate_endpoint.substr(colon + 1).c_str());
        if (p > 0 && p <= 65535) port = static_cast<std::uint16_t>(p);
    }
    res.ip = ip;
    res.port = port;

    if (friend_info.public_key.size() != 64) {
        res.status = VerifyStatus::InvalidIdentity;
        res.detail = "Expected friend public key is missing or invalid size";
        return res;
    }

    // 16-byte cryptographically secure random nonce
    const Bytes nonce = random_bytes(16);
    const std::string nonce_hex = hex(nonce);

    std::string body;
    const int http_code = http_get(ip, port, "/ctr/v1/identity?nonce=" + nonce_hex, body, timeout_ms);
    if (http_code != 200 || body.empty()) {
        res.status = VerifyStatus::NoResponse;
        res.detail = "HTTP GET /ctr/v1/identity failed or timed out (status " + std::to_string(http_code) + ")";
        return res;
    }

    auto verified = verify_friend_document(body, friend_info, nonce_hex);
    verified.ip = ip;
    verified.port = port;
    return verified;
}

VerifyResult verify_friend_document(const std::string& body, const FriendCodeInfo& friend_info,
                                    const std::string& nonce_hex) {
    VerifyResult res;
    Json doc;
    try {
        doc = parse_json(body);
    } catch (const std::exception& e) {
        res.status = VerifyStatus::MalformedResponse;
        res.detail = std::string("JSON parse error: ") + e.what();
        return res;
    }

    if (!doc.is_object()) {
        res.status = VerifyStatus::MalformedResponse;
        res.detail = "Response is not a JSON object";
        return res;
    }

    try {
    const std::string protocol = doc.at("protocol").text;
    if (protocol != "ctr-friend-auth-v1") {
        res.status = VerifyStatus::MalformedResponse;
        res.detail = "Unexpected protocol: " + protocol;
        return res;
    }

    const auto ver_val = doc.at("version").as_u64();
    if (ver_val != 1) {
        res.status = VerifyStatus::MalformedResponse;
        res.detail = "Unexpected protocol version";
        return res;
    }

    const auto server_ts = doc.at("timestamp").as_u64();
    const auto now_sec = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    // Allow up to 15 minutes of clock drift
    const std::uint64_t diff = server_ts > now_sec ? server_ts - now_sec : now_sec - server_ts;
    if (diff > 900) {
        res.status = VerifyStatus::MalformedResponse;
        res.detail = "Timestamp drift exceeds 15 minutes";
        return res;
    }

    const std::string traffic_key_b64 = doc.at("traffic_key").text;
    const std::string traffic_key_sha256 = doc.at("traffic_key_sha256").text;
    const std::string sig_hex = doc.at("signature").text;

    Bytes traffic_key;
    try {
        traffic_key = base64_decode(traffic_key_b64);
    } catch (...) {
        res.status = VerifyStatus::InvalidTrafficKey;
        res.detail = "Invalid base64 traffic key";
        return res;
    }

    if (traffic_key.size() != 0x126) {
        res.status = VerifyStatus::InvalidTrafficKey;
        res.detail = "Traffic key size is not 0x126 bytes";
        return res;
    }

    if (hex(sha256(traffic_key)) != traffic_key_sha256) {
        res.status = VerifyStatus::InvalidTrafficKey;
        res.detail = "Traffic key does not match traffic_key_sha256";
        return res;
    }

    Bytes signature;
    try {
        signature = from_hex(sig_hex);
    } catch (...) {
        res.status = VerifyStatus::InvalidIdentity;
        res.detail = "Invalid signature hex";
        return res;
    }

    if (signature.size() != 64) {
        res.status = VerifyStatus::InvalidIdentity;
        res.detail = "Signature size must be 64 bytes";
        return res;
    }

    // Authenticate signature against the EXPECTED friend public key
    const bool valid_sig = verify_friend_response(friend_info.public_key, nonce_hex, traffic_key, server_ts, signature);
    if (!valid_sig) {
        res.status = VerifyStatus::InvalidIdentity;
        res.detail = "Cryptographic signature verification failed against friend's identity";
        return res;
    }

    res.traffic_key = std::move(traffic_key);
    } catch (const std::exception& error) {
        res.status = VerifyStatus::MalformedResponse;
        res.detail = error.what();
        return res;
    }
    res.status = VerifyStatus::Verified;

    res.detail = "Server identity and game traffic key verified";
    return res;
}

VerifyResult verify_candidate_endpoint(const std::string& candidate_endpoint,
                                       const std::string& friend_code,
                                       int timeout_ms) {
    FriendCodeInfo info;
    const CodeParseError err = parse_friend_code(friend_code, info);
    if (err != CodeParseError::Ok) {
        VerifyResult res;
        res.status = VerifyStatus::InvalidIdentity;
        res.detail = code_parse_error_string(err);
        return res;
    }
    return verify_candidate_endpoint(candidate_endpoint, info, timeout_ms);
}

} // namespace ctr_online
