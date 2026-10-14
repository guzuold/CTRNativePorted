# SPDX-License-Identifier: GPL-2.0-or-later
"""Synthetic protocol controls; --capture additionally replays a local retail log."""
import argparse
import json
import struct
import unittest
from pathlib import Path
import demonware_rest as rest
import demonware_tasks as tasks


def envelope(message):
    return bytes([23, 8]) + struct.pack('<I', len(message)) + message + bytes([0])


class ProxyTests(unittest.TestCase):
    def request(self, url=b'http://abtesting/v2/users/psn-42/enrollments/?client=&context=5760'):
        header = rest.blob(1, b'Content-Type') + rest.blob(2, b'application/json')
        # Four-byte padded length, as emitted by the original client for nested objects.
        nested = bytes([34, len(header) | 128, 128, 128, 0]) + header
        return envelope(rest.blob(1, b'POST') + rest.blob(2, url) + nested)

    def test_request(self):
        service, task, request = tasks.parse_request(bytes([194, 3, 1]) + self.request())
        self.assertEqual((service, task), (194, 1))
        self.assertEqual(request['headers'], [['Content-Type', 'application/json']])
        self.assertEqual(rest.route(request)[0], 200)

    def test_reply(self):
        request = rest.parse_request(self.request())
        reply = tasks.handle(194, 1, request, {})
        self.assertEqual(reply[9:16], bytes([8, 0, 0, 0, 0, 3, 1]))
        message = reply[16:]
        self.assertEqual(message[:2], bytes([23, 8]))
        self.assertEqual(struct.unpack_from('<I', message, 2)[0], len(message) - 7)
        fields = {tag: value for tag, wire, value in rest.fields(message[6:-1])}
        self.assertEqual(fields[2], 200)
        self.assertEqual(json.loads(fields[3]), {'ABToken': 'ctr-local-ab', 'expiresIn': 3600, 'enrollments': []})

    def test_unknown_route(self):
        request = rest.parse_request(self.request(b'http://unknown/path'))
        status, body = rest.route(request)
        self.assertEqual((status, body), (404, {'error': {'name': 'NotFound'}}))

    def test_truncated_request(self):
        data = self.request()
        for size in range(len(data)):
            with self.assertRaises(ValueError):
                rest.parse_request(data[:size])

    def test_bad_fields(self):
        for data in (b'\x00', b'\x0a\x05x', b'\x08\x80', b'\x0f', b'\x08' + b'\xff' * 10):
            with self.assertRaises(ValueError):
                rest.fields(data)

    def test_unserved_task(self):
        reply = tasks.handle(38, 255, [], {})
        self.assertEqual(reply[9:14], bytes([8, 1, 0, 0, 0]))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--capture', type=Path)
    args = parser.parse_args()
    if args.capture:
        count = 0
        for event in map(json.loads, args.capture.read_text().splitlines()):
            if event.get('service') == 194 and isinstance(event['params'], list):
                raw = bytes([23]) + bytes.fromhex(event['params'][0][1])
                assert rest.route(rest.parse_request(raw))[0] == 200
                count += 1
        assert count, 'capture contains no legacy unparsed requests'
        print('Retail requests replayed:', count)
    unittest.main(argv=['test_demonware_rest.py'])
