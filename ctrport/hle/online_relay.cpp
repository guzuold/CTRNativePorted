// SPDX-License-Identifier: GPL-2.0-or-later
// Game traffic through the project server's UDP relay (see online_relay.h).
#include <winsock2.h>
#include <ws2tcpip.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include "ctr_hle.h"
#include "online_config.h"
#include "online_http.h"
#include "online_relay.h"

namespace ctr_hle::online::relay {
namespace {
using clock = std::chrono::steady_clock;
constexpr unsigned char kMagic[5] = {'C', 'T', 'R', 'R', 1};
enum : unsigned char { HELLO = 1, WELCOME = 2, SEND = 3, DELIVER = 4, REFUSED = 5 };
enum : unsigned char { NOT_REGISTERED = 1, UNKNOWN_DESTINATION = 2, DESTINATION_OFFLINE = 3, RATE_LIMITED = 4 };
constexpr auto kKeepalive = std::chrono::seconds(5);
constexpr auto kDirectAfterUnknown = std::chrono::seconds(10);
constexpr auto kDirectAfterOffline = std::chrono::seconds(2);
constexpr auto kSessionRetry = std::chrono::seconds(2);
constexpr auto kDefaultDirectProbeTimeout = std::chrono::milliseconds(1000);

std::chrono::milliseconds direct_probe_timeout() {
    static const auto value = [] {
        if (const char* v = std::getenv("CTR_ONLINE_DIRECT_PROBE_MS")) {
            const int ms = std::atoi(v);
            if (ms > 0 && ms <= 60000) return std::chrono::milliseconds(ms);
        }
        return kDefaultDirectProbeTimeout;
    }();
    return value;
}

enum class PeerRoute { ProbingDirect, Direct, Relay };

struct PeerInfo {
    PeerRoute route = PeerRoute::ProbingDirect;
    clock::time_point first_send{};
    std::uint64_t direct_sent_count = 0;
    bool logged_fallback = false;
    bool logged_direct = false;
};

bool flag(const char* name, const char* a, const char* b = nullptr) {
    const char* value = std::getenv(name);
    return value && (!std::strcmp(value, a) || (b && !std::strcmp(value, b)));
}

std::uint64_t key_of(std::uint32_t address, std::uint16_t port_network) {
    return static_cast<std::uint64_t>(address) << 16 | port_network;
}
std::string text_of(std::uint32_t address, std::uint16_t port_network) {
    return format_address(address) + ":" + std::to_string(ntohs(port_network));
}

// Number after "name": in a flat JSON object, or the string value; empty when absent.
std::string json_field(const std::string& body, const char* name) {
    const std::string key = std::string("\"") + name + "\"";
    std::size_t at = body.find(key);
    if (at == std::string::npos) return {};
    at = body.find(':', at + key.size());
    if (at == std::string::npos) return {};
    ++at;
    while (at < body.size() && body[at] == ' ') ++at;
    if (at < body.size() && body[at] == '"') {
        const std::size_t end = body.find('"', at + 1);
        return end == std::string::npos ? std::string() : body.substr(at + 1, end - at - 1);
    }
    std::size_t end = at;
    while (end < body.size() && body[end] >= '0' && body[end] <= '9') ++end;
    return body.substr(at, end - at);
}

struct State {
    std::mutex mutex;
    std::vector<unsigned char> token; // 16 bytes, empty without a session
    std::uint16_t port = 0;           // relay port, host order
    int last_status = -1;             // last relay-session answer, logged when it changes
    bool keeper = false;              // registration thread started
    struct Socket {
        clock::time_point hello{};
        bool welcomed = false;
    };
    std::map<SOCKET, Socket> sockets;
    std::map<std::uint64_t, clock::time_point> direct_until; // destination -> fallback deadline
    std::map<std::uint64_t, bool> seen_send, seen_deliver;   // first datagram per peer is logged
    std::set<std::uint64_t> logged_refusals;                 // (destination << 8 | reason) logged once
    std::map<std::uint64_t, PeerInfo> peers;                 // per-peer auto fallback tracking
    std::uint64_t relayed = 0, delivered = 0, dropped_out = 0, dropped_in = 0, direct_sent = 0;
};
State& state() {
    static State value;
    return value;
}

sockaddr_in relay_endpoint(const State& s) {
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = server_address();
    to.sin_port = htons(s.port);
    return to;
}

// HELLO from a game socket when due (lock held): every 500 ms until WELCOME, then as keepalive.
void hello_if_due(State& s, SOCKET handle, State::Socket& own) {
    if (s.token.empty()) return;
    const auto since = clock::now() - own.hello;
    if (own.welcomed ? since < kKeepalive : since < std::chrono::milliseconds(500)) return;
    const sockaddr_in to = relay_endpoint(s);
    std::vector<char> hello(kMagic, kMagic + 5);
    hello.push_back(static_cast<char>(HELLO));
    hello.insert(hello.end(), s.token.begin(), s.token.end());
    sendto(handle, hello.data(), static_cast<int>(hello.size()), 0, reinterpret_cast<const sockaddr*>(&to), sizeof(to));
    own.hello = clock::now();
}

// Registration thread: asks the relay token (the server issues it once the game's lobby
// connection is up) and keeps every attached game socket registered, so that a player who never
// sends first (the race host in a star topology) can still be reached through the relay.
void keeper() {
    State& s = state();
    for (;;) {
        bool need_token;
        {
            std::scoped_lock lock{s.mutex};
            need_token = s.token.empty() && !s.sockets.empty();
        }
        if (need_token) {
            HttpResponse response;
            const bool reached = http_exchange("POST", "/ctr/v1/relay-session", {}, "", &response);
            const std::string token = reached && response.status == 200 ? json_field(response.body, "token") : "";
            const int port = std::atoi(json_field(response.body, "port").c_str());
            std::scoped_lock lock{s.mutex};
            const int status = reached ? response.status : 0;
            if (token.size() == 32 && port > 0 && port < 65536) {
                for (std::size_t i = 0; i < 32; i += 2)
                    s.token.push_back(
                        static_cast<unsigned char>(std::strtoul(token.substr(i, 2).c_str(), nullptr, 16)));
                s.port = static_cast<std::uint16_t>(port);
                for (auto& [handle, own] : s.sockets) own = {};
            }
            if (status != s.last_status || !s.token.empty()) {
                s.last_status = status;
                ctr_hle::log_call(
                    "ctr_online", "relay_session", s.token.empty() ? 0 : 1,
                    (",\"server_status\":" + std::to_string(status) + ",\"port\":" + std::to_string(s.port)).c_str());
            }
        }
        {
            std::scoped_lock lock{s.mutex};
            for (auto& [handle, own] : s.sockets) hello_if_due(s, handle, own);
        }
        std::this_thread::sleep_for(need_token ? kSessionRetry : std::chrono::milliseconds(250));
    }
}
} // namespace

void attach(SOCKET handle) {
    if (!enabled()) return;
    State& s = state();
    std::scoped_lock lock{s.mutex};
    s.sockets[handle] = {};
    if (!s.keeper) {
        s.keeper = true;
        std::thread(keeper).detach();
    }
}

void detach(SOCKET handle) {
    State& s = state();
    std::scoped_lock lock{s.mutex};
    s.sockets.erase(handle);
}

TransportMode mode() {
    static const TransportMode value = [] {
        const char* v = std::getenv("CTR_ONLINE_NAT_TRANSPORT");
        if (v && (!std::strcmp(v, "0") || !std::strcmp(v, "direct") || !std::strcmp(v, "off")))
            return TransportMode::Off;
        if (v && (!std::strcmp(v, "relay") || !std::strcmp(v, "force"))) return TransportMode::Relay;
        return TransportMode::Auto;
    }();
    return value;
}

bool enabled() {
    return mode() != TransportMode::Off;
}
bool block_direct() {
    static const bool value = flag("CTR_ONLINE_RELAY_TEST_BLOCK_DIRECT", "1");
    return value;
}

namespace {
bool peer_with(const sockaddr_in& address, std::uint16_t relay_port) {
    const std::uint32_t a = ntohl(address.sin_addr.s_addr);
    if (a == 0 || a == 0xFFFFFFFFu || (a >> 28) == 0xE) return false; // any, broadcast, multicast
    if (a == 0x7F000002u || a == 0x7F000003u) return false;           // development discovery addresses
    const std::uint32_t public_address = server_public_address();
    if (address.sin_addr.s_addr != server_address() && (!public_address || address.sin_addr.s_addr != public_address))
        return true;
    const unsigned port = ntohs(address.sin_port);
    const unsigned discovery = known_discovery_port();
    // The server's own UDP services; a player sharing the server's address uses other ports.
    return port != 3074 && !(port >= discovery && port <= discovery + 3) && port != relay_port;
}
} // namespace

bool is_peer(const sockaddr_in& address) {
    std::uint16_t relay_port;
    {
        State& s = state();
        std::scoped_lock lock{s.mutex};
        relay_port = s.port ? s.port : 3082;
    }
    return peer_with(address, relay_port);
}

SendResult send(SOCKET handle, const char* data, int size, const sockaddr_in& destination) {
    State& s = state();
    std::scoped_lock lock{s.mutex};
    const std::uint64_t key = key_of(destination.sin_addr.s_addr, destination.sin_port);
    if (const auto it = s.direct_until.find(key); it != s.direct_until.end()) {
        if (clock::now() < it->second) return SendResult::Direct;
        s.direct_until.erase(it);
    }
    const auto own = s.sockets.find(handle);
    if (size < 0 || size > 1400 || s.token.empty() || own == s.sockets.end()) return SendResult::Direct;

    if (mode() == TransportMode::Auto) {
        PeerInfo& peer = s.peers[key];
        if (peer.route == PeerRoute::Direct) {
            ++s.direct_sent;
            return SendResult::Direct;
        }
        if (peer.route == PeerRoute::ProbingDirect) {
            const auto now = clock::now();
            if (peer.first_send == clock::time_point{}) {
                peer.first_send = now;
            }
            if (now - peer.first_send < direct_probe_timeout()) {
                ++peer.direct_sent_count;
                ++s.direct_sent;
                return SendResult::Direct;
            }
            peer.route = PeerRoute::Relay;
            if (!peer.logged_fallback) {
                peer.logged_fallback = true;
                ctr_hle::log_call("ctr_online", "relay_auto_fallback", static_cast<long long>(peer.direct_sent_count),
                                  (",\"peer\":\"" + text_of(destination.sin_addr.s_addr, destination.sin_port) +
                                   "\",\"reason\":\"direct_probe_timeout\"")
                                      .c_str());
            }
        }
    }

    const sockaddr_in to = relay_endpoint(s);
    hello_if_due(s, handle, own->second);
    std::vector<char> packet(kMagic, kMagic + 5);
    packet.push_back(static_cast<char>(SEND));
    packet.insert(packet.end(), reinterpret_cast<const char*>(&destination.sin_addr.s_addr),
                  reinterpret_cast<const char*>(&destination.sin_addr.s_addr) + 4);
    packet.insert(packet.end(), reinterpret_cast<const char*>(&destination.sin_port),
                  reinterpret_cast<const char*>(&destination.sin_port) + 2);
    packet.insert(packet.end(), data, data + size);
    if (sendto(handle, packet.data(), static_cast<int>(packet.size()), 0, reinterpret_cast<const sockaddr*>(&to),
               sizeof(to)) == SOCKET_ERROR)
        return SendResult::Direct;
    ++s.relayed;
    if (!s.seen_send[key]) {
        s.seen_send[key] = true;
        ctr_hle::log_call("ctr_online", "relay_first_send", size,
                          (",\"to\":\"" + text_of(destination.sin_addr.s_addr, destination.sin_port) +
                           "\",\"relay\":\"" + text_of(to.sin_addr.s_addr, to.sin_port) + "\"")
                              .c_str());
    }
    return SendResult::Relayed;
}

void note_dropped_out(const sockaddr_in& destination) {
    State& s = state();
    std::scoped_lock lock{s.mutex};
    if (++s.dropped_out % 500 == 1)
        ctr_hle::log_call("ctr_online", "relay_test_dropped_direct_out", static_cast<long long>(s.dropped_out),
                          (",\"to\":\"" + text_of(destination.sin_addr.s_addr, destination.sin_port) + "\"").c_str());
}

Incoming receive(SOCKET handle, const char* datagram, int size, const sockaddr_in& from, int* offset, int* payload,
                 sockaddr_in* source) {
    State& s = state();
    std::scoped_lock lock{s.mutex};
    const bool from_relay = s.port && from.sin_addr.s_addr == server_address() && ntohs(from.sin_port) == s.port;
    if (!from_relay) {
        if (block_direct() && peer_with(from, s.port ? s.port : 3082)) {
            if (++s.dropped_in % 500 == 1)
                ctr_hle::log_call("ctr_online", "relay_test_dropped_direct_in", static_cast<long long>(s.dropped_in),
                                  (",\"from\":\"" + text_of(from.sin_addr.s_addr, from.sin_port) + "\"").c_str());
            return Incoming::Dropped;
        }
        if (peer_with(from, s.port ? s.port : 3082)) {
            const std::uint64_t key = key_of(from.sin_addr.s_addr, from.sin_port);
            PeerInfo& peer = s.peers[key];
            if (mode() == TransportMode::Auto) {
                if (peer.route != PeerRoute::Direct) {
                    peer.route = PeerRoute::Direct;
                    if (!peer.logged_direct) {
                        peer.logged_direct = true;
                        ctr_hle::log_call(
                            "ctr_online", "relay_auto_direct_confirmed", 1,
                            (",\"peer\":\"" + text_of(from.sin_addr.s_addr, from.sin_port) + "\"").c_str());
                    }
                }
            }
        }
        return Incoming::Pass;
    }
    if (size < 12 || std::memcmp(datagram, kMagic, 5) != 0) return Incoming::Consumed;
    const unsigned char type = static_cast<unsigned char>(datagram[5]);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    std::memcpy(&address.sin_addr.s_addr, datagram + 6, 4);
    std::memcpy(&address.sin_port, datagram + 10, 2);
    if (type == DELIVER) {
        *offset = 12;
        *payload = size - 12;
        *source = address;
        ++s.delivered;
        const std::uint64_t key = key_of(address.sin_addr.s_addr, address.sin_port);
        if (mode() == TransportMode::Auto) {
            PeerInfo& peer = s.peers[key];
            if (peer.route != PeerRoute::Relay) {
                peer.route = PeerRoute::Relay;
                if (!peer.logged_fallback) {
                    peer.logged_fallback = true;
                    ctr_hle::log_call("ctr_online", "relay_auto_fallback", 0,
                                      (",\"peer\":\"" + text_of(address.sin_addr.s_addr, address.sin_port) +
                                       "\",\"reason\":\"peer_using_relay\"")
                                          .c_str());
                }
            }
        }
        if (!s.seen_deliver[key]) {
            s.seen_deliver[key] = true;
            ctr_hle::log_call("ctr_online", "relay_first_deliver", size - 12,
                              (",\"from\":\"" + text_of(address.sin_addr.s_addr, address.sin_port) + "\"").c_str());
        }
        return Incoming::Data;
    }
    if (type == WELCOME) {
        const auto own = s.sockets.find(handle);
        if (own != s.sockets.end() && !own->second.welcomed) {
            ctr_hle::log_call("ctr_online", "relay_welcome", 1,
                              (",\"seen_as\":\"" + text_of(address.sin_addr.s_addr, address.sin_port) + "\"").c_str());
            own->second.welcomed = true;
        }
        return Incoming::Consumed;
    }
    if (type == REFUSED && size >= 13) {
        const unsigned char reason = static_cast<unsigned char>(datagram[12]);
        if (reason == NOT_REGISTERED) {
            if (address.sin_addr.s_addr == 0) s.token.clear(); // the token itself was refused: ask again
            if (const auto own = s.sockets.find(handle); own != s.sockets.end()) own->second = {};
        } else if (reason == UNKNOWN_DESTINATION || reason == DESTINATION_OFFLINE) {
            s.direct_until[key_of(address.sin_addr.s_addr, address.sin_port)] =
                clock::now() + (reason == UNKNOWN_DESTINATION ? kDirectAfterUnknown : kDirectAfterOffline);
        }
        if (s.logged_refusals.insert(key_of(address.sin_addr.s_addr, address.sin_port) << 8 | reason).second)
            ctr_hle::log_call(
                "ctr_online", "relay_refused", reason,
                (",\"destination\":\"" + text_of(address.sin_addr.s_addr, address.sin_port) + "\"").c_str());
    }
    return Incoming::Consumed;
}
} // namespace ctr_hle::online::relay
