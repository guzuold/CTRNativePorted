// SPDX-License-Identifier: GPL-2.0-or-later
// Game traffic through the project server's UDP relay (R8 NAT traversal; wire format in
// ctrport/online_server/relay.hpp). Automatic fallback is enabled by default when online: peer
// traffic probes directly first, and falls back to the server relay if direct packets do not respond.
//
// With the variable, datagrams the game's peer socket (local UDP 3074) sends to another player
// go to the server's relay port, wrapped with the destination the game chose; datagrams the
// relay delivers are handed to the game with the address the game knows the sender by, so the
// original network code sees the same addresses as on a direct path. A background thread asks
// the relay token (POST /ctr/v1/relay-session, issued once the game's lobby connection is up)
// and registers the peer socket with HELLO, repeated every 5 s as keepalive, also when the game
// itself never sends first. When the relay refuses a destination the datagrams to it go direct
// (10 s for an unknown address, 2 s for a player not registered yet).
// Traffic to the project server's own services (discovery, relay), on its configured or its
// public address, never goes through the relay.
//
// CTR_ONLINE_RELAY_TEST_BLOCK_DIRECT=1 (tests only): every datagram the peer socket would send
// directly to a player, or receives directly from one, is dropped, as behind a NAT that lets
// nothing through. Used to prove that a race runs over the relay alone.
#pragma once
#include <winsock2.h>
#include <cstdint>

namespace ctr_hle::online::relay {
enum class TransportMode {
    Off,   // Direct only, relay disabled
    Relay, // Forced relay for all peer traffic
    Auto,  // Direct first, automatic fallback to relay if direct doesn't respond
};

TransportMode mode();
bool enabled();
bool block_direct();
// A player's endpoint, as opposed to one of the project server's services.
bool is_peer(const sockaddr_in& address);
// The game's peer socket was bound / is closing: registered with the relay while open.
void attach(SOCKET handle);
void detach(SOCKET handle);

enum class SendResult { Relayed, Direct };
// Sends through the relay from the game's socket; Direct when the direct path is active (in Auto
// mode with confirmed direct or probing), or when the relay cannot carry this datagram (no token
// yet, destination refused recently): the caller sends it directly.
SendResult send(SOCKET handle, const char* data, int size, const sockaddr_in& destination);

// Counts (and every 500th time logs) a direct datagram dropped by the test switch.
void note_dropped_out(const sockaddr_in& destination);

enum class Incoming {
    Pass,     // not relay traffic: give it to the game as received
    Data,     // relayed game datagram: payload at datagram + *offset, *payload bytes, from *source
    Consumed, // relay control message, handled here
    Dropped,  // direct peer datagram dropped by the test switch
};
Incoming receive(SOCKET handle, const char* datagram, int size, const sockaddr_in& from, int* offset, int* payload,
                 sockaddr_in* source);
} // namespace ctr_hle::online::relay
