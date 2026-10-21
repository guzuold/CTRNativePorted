// SPDX-License-Identifier: GPL-2.0-or-later
// Online configuration for R8 (active only with CTR_ONLINE=1).
//
// - CTR_ONLINE_SERVER: IPv4 address of the project's matchmaking server (default 127.0.0.1).
//   Every *.demonware.net name the game resolves is answered with this address.
// - CTR_ONLINE_ID: local identity shown to the game in place of a PSN account
//   (default "CTRPlayer"; 3-16 characters [A-Za-z0-9_-]).
//
// Network policy (AGENTS.md, plan R8): the runtime never contacts Sony or Activision. The
// resolver answers only *.demonware.net (with the project server) and numeric addresses;
// connect/sendto reach only the project server, loopback, private/link-local/CGNAT ranges,
// LAN multicast and addresses authorized by the current session authority. Everything else
// fails with EHOSTUNREACH and a log line.
#pragma once
#include <cstdint>
#include <string>

namespace ctr_hle::online {
struct Identity {
    std::string online_id;   // up to 16 characters
    std::uint64_t account_id; // nonzero: CTR_ONLINE_ACCOUNT, else derived from online_id
};
const Identity& identity();
// SHA256 room selector supplied by the launcher; never the password itself.
const std::string& room_token();
// Project server address in network byte order.
std::uint32_t server_address();
// Host's own IPv4 address used to reach the project server (network byte order), or
// loopback when it cannot be determined.
std::uint32_t local_address();
// Resolver policy: returns true and the address (network order) for allowed names.
bool resolve(const char* hostname, std::uint32_t* address, const char** reason);
// Destination policy for connect/sendto (address in network order): the project server,
// loopback, private/LAN ranges (unless CTR_ONLINE_STRICT_PEERS=1) and public addresses the
// project server confirms as assigned lobby/session peers (GET /ctr/v1/peer-allowed,
// cached for at most one second; pending session joins have a short authority lease).
bool destination_allowed(std::uint32_t address);
// UDP port of the project server's discovery service. The game sends discovery to the
// server on the fixed port 3074; when the server runs next to a game on the same PC that port
// belongs to the game, so the server announces another one (GET /ctr/v1/config, fetched once).
// discovery_port() fetches it on first use; known_discovery_port() never blocks.
std::uint16_t discovery_port();
std::uint16_t known_discovery_port();
// The project server's public IPv4 (network order) from the same /ctr/v1/config answer, or 0:
// a game on the server's own PC reaches the server on loopback but is told to probe the
// public address during discovery.
std::uint32_t server_public_address();
std::string format_address(std::uint32_t address);
} // namespace ctr_hle::online
