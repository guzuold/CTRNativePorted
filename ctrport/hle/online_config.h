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
// LAN multicast and addresses the project server has handed out (none yet). Everything else
// fails with EHOSTUNREACH and a log line.
#pragma once
#include <cstdint>
#include <string>

namespace ctr_hle::online {
struct Identity {
    std::string online_id;   // up to 16 characters
    std::uint64_t account_id; // nonzero, derived from online_id
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
// Destination policy for connect/sendto (address in network order).
bool destination_allowed(std::uint32_t address);
std::string format_address(std::uint32_t address);
} // namespace ctr_hle::online
