# SPDX-License-Identifier: GPL-2.0-or-later
"""Demonware lobby connection, protocol "210" (R8 phase 3), server side.

Contract of the game's client (retail v01.21; (development notes)):
1. client header, 28 bytes: u32 200, u32 200, u32 0xd2, u32 0xdc (offered protocols),
   u32 max receive size, client nonce [8];
2. frames, both directions: u32 size (bytes after this field) | u8 0xab | u8 type | ...
   - 0x81 server header: u32 protocol (0xd2..0xdc), connection ID u64, server nonce [8];
   - 0x82 client auth: bit buffer with the auth3 server ticket, then 8 bytes CLIENTCHAL;
   - 0x83 server auth done: 8 bytes, the second half of CLIENTCHAL;
   - 0x84 server error: u32 reason;
   - 0x85 record: u32 counter | IV [16] | AES-128-CBC(u32 length | u8 type | payload | pad)
     | HMAC-SHA1(MAC key, frame without its last 8 bytes)[:8]; counters start at 1; the server
     sends payload type 0x87 for lobby data.
3. key schedule: material = u32 0xd2 | u32 0xdc | u32 max receive size | client nonce |
   u32 (server header length + 2) | 0xab 0x81 | server header payload | client auth frame
   without its last 8 bytes; PRK = HMAC-SHA1(SHA1(material), session key) (HKDF-Extract);
   CLIENTCHAL = HKDF-Expand(PRK, "CLIENTCHAL", 16) (client proof, server proof);
   BDDATA = HKDF-Expand(PRK, "BDDATA", 72) = client MAC key [20] | server MAC key [20] |
   client AES key [16] | server AES key [16].
"""
import hashlib, hmac, os, struct

import aes128

CLIENT_HEADER_SIZE = 28
PROTOCOL = 0xd2  # lowest offered: no 220 connection migration


def hkdf_expand(prk, info, length):
    out, block, counter = b'', b'', 1
    while len(out) < length:
        block = hmac.new(prk, block + info + bytes([counter]), hashlib.sha1).digest()
        out += block
        counter += 1
    return out[:length]


def frame(message_type, payload):
    body = bytes([0xab, message_type]) + payload
    return struct.pack('<I', len(body)) + body


def parse_client_header(data):
    first, second, low, high, max_receive = struct.unpack_from('<5I', data)
    if (first, second, low, high) != (200, 200, 0xd2, 0xdc):
        raise ValueError('unexpected lobby client header')
    return max_receive, data[20:28]


class Session:
    """One lobby connection after the handshake: record encryption in both directions."""

    def __init__(self, keys):
        self.client_mac, self.server_mac = keys[0:20], keys[20:40]
        self.client_aes, self.server_aes = keys[40:56], keys[56:72]
        self.receive_counter = 1
        self.send_counter = 1

    def open_record(self, data):
        """data: frame after the size field (0xab 0x85 ...). Returns (type, payload)."""
        size = len(data)
        whole = struct.pack('<I', size) + data
        expected = hmac.new(self.client_mac, whole[:-8], hashlib.sha1).digest()[:8]
        if not hmac.compare_digest(expected, whole[-8:]):
            raise ValueError('lobby record HMAC mismatch')
        counter = struct.unpack_from('<I', data, 2)[0]
        if counter != self.receive_counter:
            raise ValueError('lobby record counter %d, expected %d' % (counter, self.receive_counter))
        self.receive_counter += 1
        iv, encrypted = data[6:22], data[22:-8]
        plain = aes128.cbc_decrypt(self.client_aes, iv, encrypted)
        length, payload_type = struct.unpack_from('<IB', plain)
        return payload_type, plain[5:5 + length]

    def seal_record(self, payload, payload_type=0x87):
        size = (len(payload) + 0x14) & ~0xf
        plain = struct.pack('<IB', len(payload), payload_type) + payload
        plain += b'\0' * (size - len(plain))
        iv = os.urandom(16)
        body = bytes([0xab, 0x85]) + struct.pack('<I', self.send_counter) + iv + \
            aes128.cbc_encrypt(self.server_aes, iv, plain)
        self.send_counter += 1
        head = struct.pack('<I', len(body) + 8) + body
        return head + hmac.new(self.server_mac, head, hashlib.sha1).digest()[:8]


class Handshake:
    def __init__(self, client_header, connection_id=None):
        self.max_receive, self.client_nonce = parse_client_header(client_header)
        self.connection_id = connection_id if connection_id is not None else \
            struct.unpack('<Q', os.urandom(8))[0]
        self.header_payload = struct.pack('<IQ', PROTOCOL, self.connection_id) + os.urandom(8)

    def server_header(self):
        return frame(0x81, self.header_payload)

    def _material(self, auth_frame):
        return struct.pack('<III', 0xd2, 0xdc, self.max_receive) + self.client_nonce + \
            struct.pack('<I', len(self.header_payload) + 2) + b'\xab\x81' + self.header_payload + \
            auth_frame[:-8]

    def complete(self, auth_frame, session_keys):
        """auth_frame: the whole 0x82 frame with its size field. Tries each candidate session
        key (bytes); returns (session key, auth done frame, Session) or None."""
        if len(auth_frame) < 14 or auth_frame[4:6] != b'\xab\x82':
            raise ValueError('not a lobby client auth frame')
        salt = hashlib.sha1(self._material(auth_frame)).digest()
        for key in session_keys:
            prk = hmac.new(salt, key, hashlib.sha1).digest()
            challenge = hkdf_expand(prk, b'CLIENTCHAL', 16)
            if hmac.compare_digest(challenge[:8], auth_frame[-8:]):
                keys = hkdf_expand(prk, b'BDDATA', 72)
                return key, frame(0x83, challenge[8:16]), Session(keys)
        return None
