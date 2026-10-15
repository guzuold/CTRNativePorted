# SPDX-License-Identifier: GPL-2.0-or-later
"""Internet-play controls: one-address discovery and peer authorization (loopback only)."""
import asyncio
import base64
import json
import socket
import struct
import tempfile
import unittest
from pathlib import Path

import ctr_online_server as server
import demonware_auth3
import demonware_tasks
import discovery
import local_matchmaking


class OneAddressDiscovery(unittest.IsolatedAsyncioTestCase):
    async def test_four_ports_and_loopback_substitution(self):
        loop = asyncio.get_running_loop()
        base = None
        for candidate in range(41000, 49000, 4):  # four consecutive free UDP ports
            probes = []
            try:
                for offset in range(4):
                    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                    probes.append(probe)
                    probe.bind(('127.0.0.1', candidate + offset))
                base = candidate
            except OSError:
                pass
            finally:
                for probe in probes:
                    probe.close()
            if base:
                break
        public = '203.0.113.7'  # TEST-NET-3, advertised only
        service = discovery.Service((public, base), (public, base + 2), base + 1, lambda **e: None,
                                    secondary_alternate_port=base + 3, public_host=public)
        transports = []
        client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        client.bind(('127.0.0.1', 0))
        client.setblocking(False)
        try:
            for advertised in service.endpoints():
                transport, _ = await loop.create_datagram_endpoint(
                    lambda advertised=advertised: service.protocol(advertised),
                    local_addr=('127.0.0.1', advertised[1]))
                transports.append(transport)
            peer = client.getsockname()
            for mode in range(4):
                await loop.sock_sendto(client, bytes([20, 2, 0, mode]), ('127.0.0.1', base))
                data, source = await asyncio.wait_for(loop.sock_recvfrom(client, 100), 1)
                self.assertEqual(source[1], [base, base + 1, base + 2, base + 3][mode])
                # Observed endpoint: the public address replaces loopback, port kept.
                self.assertEqual(data[3:7], socket.inet_aton(public))
                self.assertEqual(struct.unpack_from('<H', data, 7)[0], peer[1])
                # Secondary endpoint advertised on the public address.
                self.assertEqual(data[9:13], socket.inet_aton(public))
                self.assertEqual(struct.unpack_from('<H', data, 13)[0], base + 2)
        finally:
            client.close()
            for transport in transports:
                transport.close()

    def test_remote_sources_unchanged(self):
        self.assertEqual(discovery.observed(('198.51.100.9', 3074), '203.0.113.7'), ('198.51.100.9', 3074))
        self.assertEqual(discovery.observed(('127.0.0.1', 3074), None), ('127.0.0.1', 3074))


def advertisement(public_ip):
    block = bytes([192, 168, 1, 10, 0x02, 0x0c]) + bytes(54) + bytes(int(x) for x in public_ip.split('.')) + \
        struct.pack('<H', 3074) + b'\x01'
    assert len(block) == 67
    return json.dumps({'listen_server': {'local_address': base64.b64encode(block).decode(),
                                         'security_id': base64.b64encode(bytes(8)).decode(),
                                         'security_key': base64.b64encode(bytes(16)).decode(), 'nat': 1},
                       'dedicated_server': {'build_name': 'test'}})


class PeerAuthorization(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        demonware_tasks.matchmaking_store = local_matchmaking.Store(Path(self.directory.name) / 'm.sqlite3')
        server.internet.update(public_host='203.0.113.7', discovery_port=3078)
        server.live_clients.clear()
        self.room_a, self.room_b = 'a' * 64, 'b' * 64
        for name, room_token, ip, public in (('Host', self.room_a, '127.0.0.1', '203.0.113.7'),
                                             ('Guest', self.room_a, '198.51.100.20', '198.51.100.20'),
                                             ('Other', self.room_b, '192.0.2.44', '192.0.2.44')):
            account = demonware_auth3.user_id(name)
            server.live_clients[account] = {'user': {'room_token': room_token, 'online_id': name},
                                            'ip': server.reachable_address(ip)}
            demonware_tasks.matchmaking_store.database.execute(
                'INSERT INTO player_info VALUES (?,?)', (str(account), advertisement(public)))

    def tearDown(self):
        demonware_tasks.matchmaking_store.close()
        server.live_clients.clear()
        self.directory.cleanup()

    def ask(self, online_id, room_token, ip):
        status, _, body, _, _ = server.http_answer(
            'GET', '/ctr/v1/peer-allowed?ip=' + ip,
            {'x-ctr-online-id': online_id, 'x-ctr-room-token': room_token}, b'')
        return status, json.loads(body)['allowed']

    def test_same_room_players_allowed(self):
        self.assertEqual(self.ask('Guest', self.room_a, '203.0.113.7'), (200, True))   # co-located host
        self.assertEqual(self.ask('Host', self.room_a, '198.51.100.20'), (200, True))

    def test_other_room_or_unknown_refused(self):
        self.assertEqual(self.ask('Guest', self.room_a, '192.0.2.44'), (403, False))  # other password
        self.assertEqual(self.ask('Guest', self.room_b, '192.0.2.44'), (403, False))  # wrong room claim
        self.assertEqual(self.ask('Nobody', self.room_a, '203.0.113.7'), (403, False))  # not connected
        self.assertEqual(self.ask('Guest', self.room_a, '8.8.8.8'), (403, False))      # not a player

    def test_config_announces_discovery(self):
        status, _, body, _, _ = server.http_answer('GET', '/ctr/v1/config', {}, b'')
        self.assertEqual((status, json.loads(body)['discovery_port']), (200, 3078))


if __name__ == '__main__':
    unittest.main()
