// SPDX-License-Identifier: GPL-2.0-or-later
// CTR project server for R8, in C++: answers in place of Demonware and PSN web services.
// Port of tools/online/ctr_online_server.py (the reference implementation, kept for
// comparison); the served routes, ports and log events are the same:
// - HTTP (default 8780): /ctr/v1/traffic-key, /ctr/v1/config, /ctr/v1/peer-allowed, auth3,
//   Umbrella, PSN Web API friend list and session creation; everything else an explicit 404;
// - TCP 3074: Demonware lobby protocol 210 with tasks, matchmaking and lobby documents;
// - UDP discovery: one public address with four ports (internet), or two loopback addresses;
// - UDP 3082: relay of game datagrams between players of the same room (relay.hpp), with its
//   token from POST /ctr/v1/relay-session (C++ only: not in the Python reference);
// - recorders for other TCP/UDP ports.
// It never connects anywhere itself.
#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ctr_online {
struct ServerConfig {
    std::string host = "0.0.0.0";
    std::uint16_t http_port = 8780;
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
    // Called with each event's one-line summary (also written to stdout by the standalone server).
    std::function<void(const std::string&)> on_event;
};

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

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Project root (the directory holding runtime/) from an executable path: the first ancestor
// with a runtime/ or ctrport/ folder; falls back to the executable's directory.
std::filesystem::path find_project_root(const std::filesystem::path& exe_dir);
std::filesystem::path default_log_path(const std::filesystem::path& root);
} // namespace ctr_online
