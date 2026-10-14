# SPDX-License-Identifier: GPL-2.0-or-later
"""Local HTTP proxy tasks. No outbound HTTP is performed."""
import json
import re
import struct
from urllib.parse import urlsplit


def varint(value):
    if not 0 <= value < 1 << 64:
        raise ValueError('varint outside uint64')
    out = bytearray()
    while value >= 128:
        out.append((value & 127) | 128)
        value >>= 7
    out.append(value)
    return bytes(out)


def read_varint(data, offset):
    value = 0
    for shift in range(0, 70, 7):
        if offset >= len(data):
            raise ValueError('truncated varint')
        byte = data[offset]
        offset += 1
        value |= (byte & 127) << shift
        if not byte & 128:
            if value >= 1 << 64:
                raise ValueError('varint overflow')
            return value, offset
    raise ValueError('varint overflow')


def fields(data):
    result, offset = [], 0
    while offset < len(data):
        key, offset = read_varint(data, offset)
        tag, wire = key >> 3, key & 7
        if not tag:
            raise ValueError('zero field tag')
        if wire == 0:
            value, offset = read_varint(data, offset)
        else:
            if wire == 2:
                size, offset = read_varint(data, offset)
            elif wire in (1, 5):
                size = 8 if wire == 1 else 4
            else:
                raise ValueError('unsupported wire type')
            if size > len(data) - offset:
                raise ValueError('truncated field')
            value = data[offset:offset + size]
            offset += size
        result.append((tag, wire, value))
    return result


def uint(tag, value):
    return varint(tag << 3) + varint(value)


def blob(tag, value):
    return varint((tag << 3) | 2) + varint(len(value)) + value


def parse_request(data):
    # Observed retail request: struct marker 23, typed u32 size, fields, end marker 0.
    if len(data) < 7 or data[:2] != bytes([23, 8]):
        raise ValueError('unsupported HTTP proxy envelope')
    size = struct.unpack_from('<I', data, 2)[0]
    if len(data) != size + 7 or data[-1] != 0:
        raise ValueError('invalid HTTP proxy envelope length')
    values = fields(data[6:-1])
    def text(tag):
        matches = [v for t, w, v in values if t == tag and w == 2]
        if len(matches) != 1:
            raise ValueError('missing or duplicate HTTP field')
        return matches[0].decode('utf-8')
    headers = []
    for tag, wire, value in values:
        if tag == 4 and wire == 2:
            header = {t: v.decode('utf-8') for t, w, v in fields(value) if w == 2}
            if 1 not in header or 2 not in header:
                raise ValueError('invalid HTTP header')
            headers.append([header[1], header[2]])
    return {'method': text(1), 'url': text(2), 'headers': headers}


def route(request):
    url = urlsplit(request['url'])
    if (request['method'] == 'POST' and url.scheme == 'http' and url.netloc == 'abtesting'
            and re.fullmatch(r'/v2/users/psn-[0-9]+/enrollments/', url.path)):
        return 200, {'ABToken': 'ctr-local-ab', 'expiresIn': 3600, 'enrollments': []}
    return 404, {'error': {'name': 'NotFound'}}


def response(request):
    status, body = route(request)
    header = blob(1, b'Content-Type') + blob(2, b'application/json')
    message = blob(1, header) + uint(2, status) + blob(3, json.dumps(body, separators=(',', ':')).encode())
    return bytes([23, 8]) + struct.pack('<I', len(message)) + message + bytes([0])
