// SPDX-License-Identifier: GPL-2.0-or-later
// Online pieces used by the launcher: hosting the project server inside the launcher process
// (with automatic router port forwarding), resolving the server name a player typed, and
// fetching the server's traffic key fresh at each launch.
#pragma once
#include "upnp.hpp"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ctr_online {
class Server;

// UDP port offset of the hosting player's own game (3074 + 100): its public address is the
// server's, where UDP 3074 means the server's discovery to every player's runtime.
constexpr int kHostGamePortOffset = 100;

// Ports a host must receive from the internet: the server's HTTP and lobby, its discovery and
// relay, and the hosting player's game UDP port.
std::vector<PortRequest> host_ports(std::uint16_t http_port = 8780, std::uint16_t discovery_port = 3078,
                                    std::uint16_t game_udp_port = 3074 + kHostGamePortOffset);

struct HostStatus {
    bool running = false;
    std::string public_ip;       // address other players type as server
    std::string address_source;  // "upnp", "manual" or "lan"
    std::string lan_ip;
    bool upnp_router = false;    // a router answered UPnP
    std::vector<PortResult> ports;
    std::vector<std::string> listen_failures;
    std::vector<std::string> warnings;
    std::string error;           // set when the server could not start
    std::filesystem::path traffic_key;
    std::filesystem::path log;
};

class Host {
public:
    Host();
    ~Host();
    // Blocking (UPnP discovery takes a few seconds): call it off the UI thread.
    // manual_public_ip: used as is when not empty (no UPnP address lookup, ports still mapped).
    HostStatus start(const std::filesystem::path& root, const std::string& manual_public_ip, bool use_upnp);
    void stop();
    bool running() const;
    HostStatus status() const;
    std::size_t players() const;

private:
    mutable std::mutex mutex_;
    std::unique_ptr<Server> server_;
    std::unique_ptr<PortMapper> mapper_;
    HostStatus status_;
};

// IPv4 text for a server name or address ("" when it cannot be resolved). The runtime only
// accepts a numeric IPv4 address.
std::string resolve_ipv4(const std::string& host);
// Plain HTTP GET (no proxy, no cache). Returns the HTTP status, 0 when unreachable.
int http_get(const std::string& ipv4, std::uint16_t port, const std::string& path, std::string& body, int timeout_ms = 4000);
} // namespace ctr_online
