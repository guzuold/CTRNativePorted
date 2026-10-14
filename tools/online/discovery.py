# SPDX-License-Identifier: GPL-2.0-or-later
"""Project UDP endpoint discovery, using the sender observed by the real socket.

No NAT class is asserted: the original client runs its tests against four actual
source endpoints. Only the observed protocol versions 2 (NAT) and 3 (IP) are served.
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


def answer(data, peer, primary, secondary, alternate_port):
    if data == bytes([30, 3, 0]):
        return primary, bytes([31, 3, 0]) + address(peer) + address(peer, True)
    if len(data) == 4 and data[:3] == bytes([20, 2, 0]) and data[3] <= 3:
        request = data[3]
        source = (secondary[0] if request & 2 else primary[0],
                  alternate_port if request & 1 else primary[1])
        return source, bytes([21, 2, 0]) + address(peer) + address(secondary)
    return None


class Service:
    def __init__(self, primary, secondary, alternate_port, record):
        self.primary, self.secondary, self.alternate_port = primary, secondary, alternate_port
        self.record = record
        self.transports = {}

    def protocol(self, endpoint):
        service = self
        class Protocol(asyncio.DatagramProtocol):
            def connection_made(self, transport):
                service.transports[endpoint] = transport

            def datagram_received(self, data, peer):
                service.record(event='udp_data', port=endpoint[1], peer='%s:%d' % peer,
                               length=len(data), hex=data.hex())
                result = answer(data, peer, service.primary, service.secondary, service.alternate_port)
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
