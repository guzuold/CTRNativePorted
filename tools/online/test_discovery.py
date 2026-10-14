# SPDX-License-Identifier: GPL-2.0-only
"""UDP source-endpoint controls and malformed-request rejection (loopback only)."""
import asyncio
import socket
import struct
import unittest
from discovery import Service, answer


class DiscoveryTests(unittest.IsolatedAsyncioTestCase):
    async def test_actual_source_endpoints(self):
        loop = asyncio.get_running_loop()
        sockets, transports = [], []
        client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        client.bind(('127.0.0.1', 0)); client.setblocking(False)
        try:
            for host, port in [('127.0.0.2', 0), ('127.0.0.2', 0)]:
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); sock.bind((host, port)); sockets.append(sock)
            primary, alternate = [s.getsockname() for s in sockets]
            secondary = ('127.0.0.3', primary[1])
            for endpoint in [secondary, ('127.0.0.3', alternate[1])]:
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); sock.bind(endpoint); sockets.append(sock)
            service = Service(primary, secondary, alternate[1], lambda **event: None)
            for sock in sockets:
                endpoint = sock.getsockname()
                transport, protocol = await loop.create_datagram_endpoint(lambda endpoint=endpoint: service.protocol(endpoint), sock=sock)
                transports.append(transport)
            peer = client.getsockname()
            for mode, expected in enumerate([primary, alternate, secondary, ('127.0.0.3', alternate[1])]):
                await loop.sock_sendto(client, bytes([20,2,0,mode]), primary)
                data, source = await asyncio.wait_for(loop.sock_recvfrom(client, 100), 1)
                self.assertEqual(source, expected)
                self.assertEqual(data[:7], bytes([21,2,0,127,0,0,1]))
                self.assertEqual(struct.unpack_from('<H',data,7)[0], peer[1])
            await loop.sock_sendto(client, bytes([30,3,0]), primary)
            data, source = await asyncio.wait_for(loop.sock_recvfrom(client, 100), 1)
            self.assertEqual(source, primary)
            self.assertEqual(data[:3], bytes([31,3,0]))
            # Apply the original client's native uint32 decoding, not the encoder's formula.
            decoded = (int.from_bytes(data[9:13], 'little') ^ 0x2112a442).to_bytes(4, 'little')
            self.assertEqual(decoded, socket.inet_aton(peer[0]))
            self.assertEqual(struct.unpack_from('<H',data,13)[0] ^ 0x2112, peer[1])
        finally:
            client.close()
            for transport in transports: transport.close()
            for sock in sockets: sock.close()

    def test_unknown_or_truncated_requests(self):
        for data in (b'', b'\x1e', b'\x1e\x03', b'\x1e\x03\x00\x00', b'\x14\x02\x00\x04', b'\x14\x01\x00\x00'):
            self.assertIsNone(answer(data, ('127.0.0.1', 3174), ('127.0.0.2', 3074), ('127.0.0.3', 3074), 3076))


if __name__ == '__main__': unittest.main()
