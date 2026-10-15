# SPDX-License-Identifier: GPL-2.0-or-later
"""Project UDP endpoint discovery, using the sender observed by the real socket.

No NAT class is asserted: the original client runs its tests against four actual
source endpoints. Only the observed protocol versions 2 (NAT) and 3 (IP) are served.

Two layouts:
- two addresses (development on one PC): primary and secondary are different IPs, both
  using the same alternate port;
- one public address (internet): the "other address" endpoints are emulated with two
  further ports of the same IP (secondary port and secondary alternate port). A client
  behind a NAT that filters by address only then looks less restrictive than it is; one
  that filters by port is still detected.
Replies always go out from the endpoint the request selects. When the server runs on the
same PC as a game, that game reaches it over loopback: `public_host` replaces a loopback
source in the observed endpoint, so the game advertises the address other players can reach.
"""
import asyncio
import ipaddress
import struct


def address(endpoint, protected=False):
    ip, port = endpoint
    value = int(ipaddress.IPv4Address(ip))
    if protected:
        # The original x86 client XORs the in-memory IPv4 uint32, without ntohl.
        # Its byte mask is 42 a4 12 21, unlike standard STUN's network-order cookie.
        value ^= 0x42a41221
        port ^= 0x2112
    return value.to_bytes(4, 'big') + struct.pack('<H', port)


def observed(peer, public_host=None):
    """Endpoint to report for a request from `peer`."""
    if public_host and ipaddress.IPv4Address(peer[0]).is_loopback:
        return public_host, peer[1]
    return peer[0], peer[1]


def answer(data, peer, primary, secondary, alternate_port, secondary_alternate_port=None,
           public_host=None):
    """Returns (source endpoint, reply) or None. Endpoints are the advertised ones."""
    seen = observed(peer, public_host)
    if data == bytes([30, 3, 0]):
        return primary, bytes([31, 3, 0]) + address(seen) + address(seen, True)
    if len(data) == 4 and data[:3] == bytes([20, 2, 0]) and data[3] <= 3:
        request = data[3]
        change_address, change_port = request & 2, request & 1
        if secondary_alternate_port is None:
            secondary_alternate_port = alternate_port
        base = secondary if change_address else primary
        port = (secondary_alternate_port if change_address else alternate_port) if change_port else base[1]
        return (base[0], port), bytes([21, 2, 0]) + address(seen) + address(secondary)
    return None


class Service:
    def __init__(self, primary, secondary, alternate_port, record, secondary_alternate_port=None,
                 public_host=None):
        self.primary, self.secondary, self.alternate_port = primary, secondary, alternate_port
        self.secondary_alternate_port = secondary_alternate_port
        self.public_host = public_host
        self.record = record
        self.transports = {}

    def endpoints(self):
        """The four advertised source endpoints, in request order 0..3."""
        sa = self.alternate_port if self.secondary_alternate_port is None else self.secondary_alternate_port
        return [self.primary, (self.primary[0], self.alternate_port), self.secondary,
                (self.secondary[0], sa)]

    def protocol(self, endpoint):
        """Protocol for the socket answering as advertised `endpoint` (bound anywhere)."""
        service = self
        class Protocol(asyncio.DatagramProtocol):
            def connection_made(self, transport):
                service.transports[endpoint] = transport

            def datagram_received(self, data, peer):
                service.record(event='udp_data', port=endpoint[1], peer='%s:%d' % peer,
                               length=len(data), hex=data.hex())
                result = answer(data, peer, service.primary, service.secondary,
                                service.alternate_port, service.secondary_alternate_port,
                                service.public_host)
                if result:
                    source, reply = result
                    transport = service.transports.get(source)
                    if not transport:
                        service.record(event='discovery_unavailable', source=source)
                        return
                    transport.sendto(reply, peer)
                    service.record(event='discovery_reply', peer='%s:%d' % peer,
                                   source=source, request=data.hex(), reply=reply.hex())
        return Protocol()
