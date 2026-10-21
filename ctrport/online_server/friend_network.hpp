// SPDX-License-Identifier: GPL-2.0-or-later
// Phase 2 online: the players' local services talk to each other directly, without a shared
// server. Each game stays connected to its own local service; the services exchange signed
// messages at POST /ctr/v1/peer.
//
// Packet: {code, payload, time, signature}. The sender signs "ctr-peer-v1:" + sha256(payload)
// with its friend identity (the friend code holds the public key). The payload carries
// "action", a random "nonce" and "to", the recipient's friend code, so a packet cannot be
// replayed to another service. The answer is signed by the recipient and repeats the nonce.
// Packets older or newer than 60 seconds are refused and a nonce is accepted once.
// Services answer only friends: each side must have the other in its rubrica (friends.json).
#pragma once
#include "friend_identity.hpp"
#include "json.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace ctr_online {

Json sign_peer_message(const HostIdentity& identity, const Json& payload);
// Returns the payload when the packet is signed by expected_code within 60 s of now.
Json verify_peer_message(const Json& packet, const std::string& expected_code, std::uint64_t now);
// Friend code a packet claims to come from (unverified: verify_peer_message checks it).
std::string peer_message_sender(const Json& packet);

// Client side: signs the payload (adds nonce and "to"), posts it to ip:port and returns the
// verified answer from target_code. Throws on transport, signature or nonce failure, and with
// the remote "error" text when the service refuses the request.
Json peer_request(const HostIdentity& self, const std::string& ip, std::uint16_t port,
                  const std::string& target_code, Json payload, int timeout_ms = 3000);

// Server-side replay guard for (sender, nonce) pairs, kept for 120 seconds.
class PeerNonces {
public:
    bool fresh(const std::string& sender, const std::string& nonce, std::uint64_t now);
private:
    std::mutex mutex_;
    std::map<std::string, std::uint64_t> seen_;
};

// What a friend's service last reported.
struct FriendState {
    std::string code;
    std::string nickname;          // name saved in the rubrica
    std::string name;              // online ID reported by the friend's service ("" until contact)
    std::uint64_t account = 0;     // player_account(code, name), the account the game uses
    bool online = false;           // the friend's service answered the last poll
    bool in_game = false;          // its game is connected to it
    std::string ip;
    std::uint16_t port = 8780;
    std::uint64_t seen = 0;        // unix time of the last answer
    Json report = Json::object();  // last presence answer (sessions, delta records)
};

// Keeps the presence of every friend of the rubrica current through the peer channel. The
// last online ID each friend reported is saved next to the rubrica (friend_names.json), so the
// in-game list keeps offline friends across restarts.
class FriendWatcher {
public:
    // Endpoint of a friend's service (ip, port), or nullopt when it cannot be found now.
    using Resolver = std::function<std::optional<std::pair<std::string, std::uint16_t>>(const std::string& code)>;
    FriendWatcher(const HostIdentity& self, std::filesystem::path friends_path, Resolver resolver,
                  std::chrono::milliseconds period = std::chrono::seconds(5));
    ~FriendWatcher();
    FriendWatcher(const FriendWatcher&) = delete;
    FriendWatcher& operator=(const FriendWatcher&) = delete;
    void start();
    void stop();
    void poll_now();                                 // one synchronous round
    std::vector<FriendState> friends() const;        // rubrica order, also offline friends
    std::optional<FriendState> by_code(const std::string& code) const;
    std::optional<FriendState> by_account(std::uint64_t account) const;
    bool is_friend(const std::string& code) const;   // in the rubrica now
    // Hint from a signed, authorized request; never marks the sender online.
    void observe_endpoint(const std::string& code, const std::string& ip, std::uint16_t port);
    std::uint16_t contact_port = 8780;
    std::function<void(const std::string&, const std::string&)> on_failure;
    // Sends an action to a friend's service (endpoint from the last poll or the resolver).
    Json request(const std::string& code, Json payload, int timeout_ms = 3000);
    // Called after a round in which some friend's state changed.
    std::function<void()> on_change;
    // This service's player, sent with each presence request: the friend's service reports the
    // sessions that player may see.
    std::function<std::string()> local_name;
private:
    std::optional<std::pair<std::string, std::uint16_t>> endpoint_of(const std::string& code);
    void run();
    const HostIdentity& self_;
    std::filesystem::path friends_path_;
    std::filesystem::path names_path_; // friend_names.json: last online ID of each friend
    Resolver resolver_;
    std::chrono::milliseconds period_;
    std::mutex poll_mutex_;
    mutable std::mutex mutex_;
    std::map<std::string, FriendState> states_;
    std::map<std::string, std::pair<std::string, std::uint16_t>> observed_;
    std::vector<std::string> order_;
    std::mutex wake_mutex_;
    std::condition_variable wake_;
    std::atomic<bool> stopping_{false};
    std::thread thread_;
};

// Endpoint resolver from the rubrica's last verified endpoint ("ip:port").
FriendWatcher::Resolver rubrica_resolver(const std::filesystem::path& friends_path);

} // namespace ctr_online
