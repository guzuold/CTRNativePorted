// SPDX-License-Identifier: GPL-2.0-or-later
// UDP relay of the project server (R8 NAT traversal). Game datagrams between players of the
// same room travel through the server when they cannot reach each other directly (NAT,
// symmetric NAT, CGNAT). It helps only when the server itself is reachable from the internet.
//
// Wire format, our own (multi-byte fields big-endian, IPv4 addresses in network order):
//   header   "CTRR" | version 1 | type                          (6 bytes)
//   HELLO    (1) client -> server: token[16]                    registers the sender's endpoint
//   WELCOME  (2) server -> client: ip[4] port[2] lease_s[2]     the endpoint the server saw
//   SEND     (3) client -> server: ip[4] port[2] payload        the game's destination address
//   DELIVER  (4) server -> client: ip[4] port[2] payload        the address the receiving game
//                                                               knows the sender by
//   REFUSED  (5) server -> client: ip[4] port[2] reason[1]
// The token comes from POST /ctr/v1/relay-session (same identity, room and IP as the player's
// lobby connection). Only players online in the same room reach each other; each binding has a
// byte-rate limit. HELLO is repeated as keepalive (it also keeps the NAT binding open).
#pragma once
#include "crypto.hpp"
#include "json.hpp"
#include "protocol.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ctr_online {
namespace relay {
constexpr unsigned char kVersion = 1;
enum Type : unsigned char { HELLO = 1, WELCOME = 2, SEND = 3, DELIVER = 4, REFUSED = 5 };
enum Reason : unsigned char {
    NOT_REGISTERED = 1,      // unknown token, unknown or expired sender endpoint, sender left the room
    UNKNOWN_DESTINATION = 2, // no player of the sender's room has that address
    DESTINATION_OFFLINE = 3, // the player exists but has no live relay binding
    RATE_LIMITED = 4,
    MALFORMED = 5,
};
constexpr std::size_t kHeader = 6, kAddressed = 12, kTokenSize = 16, kMaxPayload = 1400;
Bytes encode(Type type, const Endpoint& address, const Bytes& payload = {});
Bytes hello(const Bytes& token);
const char* reason_name(unsigned char reason);
} // namespace relay

struct RelayMember {
    std::uint64_t account = 0;
    std::optional<Endpoint> public_endpoint, local_endpoint; // from the 67-byte advertisement block
};
// Players of `room` online now (lobby connection open). Called for each datagram.
using RelayRoster = std::function<std::vector<RelayMember>(const std::string& room)>;

// Endpoint stored in an advertisement block: ip[4] at offset, port little-endian at offset+4.
// nullopt for a zero address or port.
std::optional<Endpoint> block_endpoint(const Bytes& block, std::size_t offset);

struct RelayLimits {
    double bytes_per_second = 65536, burst = 131072;
    double binding_timeout = 60; // seconds without datagrams before a binding expires
};

class Relay {
public:
    explicit Relay(RelayLimits limits = {});
    // A new token for HELLO; replaces the account's earlier token and binding.
    Bytes issue(std::uint64_t account, const std::string& room);
    void forget(std::uint64_t account);
    struct Output {
        Endpoint to;
        Bytes data;
    };
    struct Outcome {
        std::vector<Output> out;
        std::string event; // empty: nothing to log (ordinary forwarding)
        Json fields = Json::object();
    };
    Outcome handle(const Bytes& datagram, const Endpoint& from, double now, const RelayRoster& roster);
    struct Stats {
        std::uint64_t forwarded = 0, forwarded_bytes = 0, refused = 0;
    };
    Stats stats() const;
    const RelayLimits& limits() const { return limits_; }

private:
    struct Binding {
        Endpoint endpoint;
        std::string room;
        double last = 0, tokens = 0, refill = 0, last_refusal_log = -1e9;
        std::uint64_t forwarded = 0, bytes = 0;
    };
    struct Grant {
        std::uint64_t account;
        std::string room;
    };
    Outcome refuse(const Endpoint& to, const Endpoint& address, unsigned char reason);
    void drop_binding(std::uint64_t account);

    RelayLimits limits_;
    mutable std::mutex mutex_;
    std::map<std::string, Grant> tokens_;              // token hex -> grant
    std::map<std::uint64_t, std::string> token_of_;    // account -> token hex
    std::map<std::uint64_t, Binding> bindings_;        // account -> binding
    std::map<std::string, std::uint64_t> by_endpoint_; // endpoint text -> account
    // (sender, peer) -> the address the sender's game used for the peer.
    std::map<std::pair<std::uint64_t, std::uint64_t>, Endpoint> aliases_;
    Stats stats_;
};
} // namespace ctr_online
