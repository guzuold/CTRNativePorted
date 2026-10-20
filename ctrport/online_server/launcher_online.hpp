// SPDX-License-Identifier: GPL-2.0-or-later
// Online pieces used by the launcher: hosting the project server inside the launcher process
// (with automatic router port forwarding), resolving the server name a player typed, and
// fetching the server's traffic key fresh at each launch.
#pragma once
#include "upnp.hpp"
#include "json.hpp"
#include "friend_verifier.hpp"
#include "friend_network.hpp"
#include "server.hpp"
#include <atomic>
#include <condition_variable>
#include <optional>
#include <thread>
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
    std::string friend_code;
};

class DhtService;

// DHT key on which the services that can host public matchmaking announce themselves.
Bytes public_matchmaking_key();
// Hub election: the lowest friend code among the verified candidates and, when it can be
// reached from the internet, this service itself. nullopt when there is no candidate.
std::optional<MatchHub> choose_hub(const MatchHub& self, bool self_reachable, const std::vector<MatchHub>& verified);

class Host {
public:
    Host();
    ~Host();
    // Blocking (UPnP discovery takes a few seconds): call it off the UI thread.
    // manual_public_ip: used as is when not empty (no UPnP address lookup, ports still mapped).
    // local_name: the player's online ID, reported to the friends of the rubrica, whose services
    // this one polls for presence (friend_network.hpp).
    HostStatus start(const std::filesystem::path& root, const std::string& manual_public_ip, bool use_upnp,
                     const std::string& local_name = {});
    void stop();
    bool running() const;
    HostStatus status() const;
    std::size_t players() const;
    DhtService* dht() const;
    // Friends of the rubrica with their last presence (empty while stopped).
    std::vector<FriendState> friends() const;
    // Current public matchmaking hub (this service's own code when it hosts or plays alone).
    std::optional<MatchHub> match_hub() const;

private:
    void elect_hubs(std::filesystem::path root, MatchHub self, bool self_reachable);
    mutable std::mutex hub_mutex_;
    std::condition_variable hub_wake_;
    bool hub_stop_ = false;
    std::optional<MatchHub> hub_;
    std::thread hub_thread_;
    mutable std::mutex mutex_;
    std::unique_ptr<Server> server_;
    std::unique_ptr<PortMapper> mapper_;
    std::unique_ptr<DhtService> dht_;
    HostStatus status_;
};

// Gets the local player's friend code from runtime/user/identity/host_identity.json (created if missing).
std::string local_friend_code(const std::filesystem::path& root);

// Searches the DHT and verifies candidates against the given friend code.
// Returns VerifyResult (status Verified, NoResponse, InvalidIdentity, etc.).
VerifyResult find_friend_server(const std::filesystem::path& root,
                                const std::string& friend_code,
                                int dht_timeout_ms = 8000,
                                int verify_timeout_ms = 4000,
                                const std::vector<std::string>& extra_bootstrap_nodes = {},
                                DhtService* shared_dht = nullptr);

struct PartySelection {
    std::string ip, owner, room;
    Json snapshot = Json::object();
};
Json party_request(const std::filesystem::path& root, const std::string& ip, const std::string& code, Json request);
// Call after repeated heartbeat failures, using the last authenticated snapshot.
PartySelection recover_party(const std::filesystem::path& root, const PartySelection& previous,
                             const std::string& local_ip = "127.0.0.1");
// Pull invitations from a known lobby host through an outbound connection.
// The destination never needs a listening public port. Returns false if host lookup fails.
bool poll_party_invitations(const std::filesystem::path& root, const std::string& host_code,
                           DhtService* shared_dht, int lookup_timeout_ms = 2000);

// IPv4 text for a server name or address ("" when it cannot be resolved). The runtime only
// accepts a numeric IPv4 address.
std::string resolve_ipv4(const std::string& host);
// Plain HTTP GET (no proxy, no cache). Returns the HTTP status, 0 when unreachable.
int http_get(const std::string& ipv4, std::uint16_t port, const std::string& path, std::string& body, int timeout_ms = 4000);
} // namespace ctr_online
