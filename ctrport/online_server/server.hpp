// SPDX-License-Identifier: GPL-2.0-or-later
// CTR project server for R8, in C++: answers in place of Demonware and PSN web services.
// Port of tools/online/ctr_online_server.py (the reference implementation, kept for
// comparison); the served routes, ports and log events are the same:
// - HTTP (default 8780): /ctr/v1/traffic-key, /ctr/v1/config, /ctr/v1/peer-allowed, auth3,
//   Umbrella, PSN Web API friend list, session creation and service entitlements;
//   everything else an explicit 404;
// - TCP 3074: Demonware lobby protocol 210 with tasks, matchmaking, lobby documents and the
//   Pit Stop / Wumpa Challenge state (stores.hpp);
// - UDP discovery: one public address with four ports (internet), or two loopback addresses;
// - UDP 3082: relay of game datagrams between players of the same room (relay.hpp), with its
//   token from POST /ctr/v1/relay-session (C++ only: not in the Python reference);
// - recorders for other TCP/UDP ports.
// It never connects anywhere itself.
#pragma once
#include "friend_network.hpp"
#include "json.hpp"
#include <cstdint>
#include <filesystem>
#include <functional>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ctr_online {
// The service whose public matchmaking a player uses (phase 2: one hub elected among the
// players' own services).
struct MatchHub {
    std::string code, ip;
    std::uint16_t port = 8780;
};

struct ServerConfig {
    std::string host = "0.0.0.0";
    std::uint16_t http_port = 8780;
    std::uint16_t lobby_port = 3074; // Allows isolated local protocol fixtures.
    std::vector<std::uint16_t> tcp_record_ports{3075};
    std::vector<std::uint16_t> udp_record_ports{3075, 3478, 3479};
    // Two-address discovery (development on one PC), used without public_host.
    std::string peer_host = "127.0.0.2";
    std::string stun_secondary_host = "127.0.0.3";
    std::uint16_t stun_alternate_port = 3076;
    // Internet: public IPv4, one-address discovery on discovery_port..discovery_port+3.
    std::string public_host;
    std::uint16_t discovery_port = 0; // 0: 3078 with public_host
    // UDP relay between players of the same room (relay.hpp); 0 turns it off.
    std::uint16_t relay_port = 3082;
    std::filesystem::path state_dir;  // runtime/user/online
    std::filesystem::path log_path;   // runtime/user/logs/online_server_<time>.jsonl
    bool party_enabled = false; // launcher-managed membership and admission
    std::filesystem::path identity_path; // optional custom path for host_identity.json
    // Friend network (phase 2): answer the signed POST /ctr/v1/peer of the friends in
    // state_dir/friends.json and poll their services for presence (friend_network.hpp).
    bool friend_network = false;
    std::string local_name; // online ID of this PC's player; else the first connected game
    FriendWatcher::Resolver friend_resolver; // empty: the rubrica's last verified endpoint
    std::chrono::milliseconds friend_poll_period{5000};
    // Public matchmaking across PCs: the hub to use, asked when a player of this service starts
    // a public search (145/2, 145/3, 145/15, 145/18); nullopt or this service's own code: match
    // here. The hub then stays fixed for that player until its game disconnects or the hub fails.
    std::function<std::optional<MatchHub>()> match_hub;
    // Optional bounded demand election, only for a nonreachable launcher with no hub.
    std::function<std::optional<MatchHub>(std::chrono::milliseconds)> await_match_hub;
    bool hub_reachable = true;
    std::chrono::milliseconds hub_wait_budget{1000};
    // Called with each event's one-line summary (also written to stdout by the standalone server).
    std::function<void(const std::string&)> on_event;
};

class HostIdentity;

class Server {
public:
    explicit Server(ServerConfig config);
    ~Server();
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    // Opens every listener it can; returns the names of those listening (failures are logged).
    std::vector<std::string> start();
    void stop();
    const std::vector<std::string>& failures() const;
    std::filesystem::path traffic_key_path() const;
    std::size_t connected_players() const;
    std::string friend_code() const;
    const HostIdentity* host_identity() const;
    // Friends of the rubrica with their last presence (empty without friend_network).
    std::vector<FriendState> friends() const;
    void record_event(const char* name, Json fields);
    void poll_friends(); // one synchronous presence round

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Project root (the directory holding runtime/) from an executable path: the first ancestor
// with a runtime/ or ctrport/ folder; falls back to the executable's directory.
// GET sdk:entitlement/v1/users/me/entitlements?<query>: the empty service entitlement list,
// or nullopt for a request other than start>=0, 0<size<=1000, integer service_label and
// entitlement_type=service (the reference server's 400).
std::optional<Json> entitlement_page(const std::string& query);

std::filesystem::path find_project_root(const std::filesystem::path& exe_dir);
std::filesystem::path default_log_path(const std::filesystem::path& root);
} // namespace ctr_online
