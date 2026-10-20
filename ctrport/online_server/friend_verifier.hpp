// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "friend_identity.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace ctr_online {

enum class VerifyStatus {
    Verified = 0,
    NoResponse,
    InvalidIdentity,
    MalformedResponse,
    InvalidTrafficKey
};

const char* verify_status_string(VerifyStatus status);

struct VerifyResult {
    VerifyStatus status = VerifyStatus::NoResponse;
    std::string ip;
    std::uint16_t port = 0;
    Bytes traffic_key; // Der SPKI if verified
    std::string detail;
};

// Checks an untrusted identity response with the caller's fresh challenge.
VerifyResult verify_friend_document(const std::string& body, const FriendCodeInfo& friend_info,
                                    const std::string& nonce_hex);

// Verifies a candidate server endpoint against the expected friend code / public key.
// Performs HTTP challenge-response handshake over TCP on candidate port (default 8780).
// Call off UI thread.
VerifyResult verify_candidate_endpoint(const std::string& candidate_endpoint,
                                       const FriendCodeInfo& friend_info,
                                       int timeout_ms = 4000);

VerifyResult verify_candidate_endpoint(const std::string& candidate_endpoint,
                                       const std::string& friend_code,
                                       int timeout_ms = 4000);

// A service whose code is not known in advance (public matchmaking candidates): the code the
// identity answer states, verified against that same answer's signature of a fresh nonce.
// `code` is set only when the result is Verified.
VerifyResult identify_endpoint(const std::string& candidate_endpoint, std::string& code, int timeout_ms = 2000);

} // namespace ctr_online
