# SPDX-License-Identifier: GPL-2.0-or-later
"""auth3 replies of the project server (R8 phase 3).

Contract read from the retail v01.21 eboot ((development notes)):
- the PS4 client posts {"auth_task":"46", "iv_seed", "title_id", "identity", "extra_data"}
  and calls the reply handler expecting task 47 (0x2f) and a 0x80-byte client ticket;
- the reply must be HTTP 200 with an "X-Signature" header: base64 RSASSA-PSS/SHA-256
  (salt length 0) of the whole body, verified with the traffic key in the eboot, which the
  runtime replaces with this server's key (ctrport/online_traffic_key.hpp);
- body: "auth_task" 47, "code" 700, "iv_seed", "client_ticket" (base64 of 0x80 bytes; a
  ticket that already starts with the magic number 0xEFBDADDE is used as is), "server_ticket"
  (base64 of 0x80 bytes, kept by the client for the lobby login), "account_type",
  "client_id" and "extra_data" (a JSON string whose "extended_data" the client stores);
- no "identity" key, so the client takes the session key from the ticket as is.

Client ticket layout (little endian, packed): magic u32 | type u8 | title ID u32 |
issued u32 | expires u32 | license ID u64 | user ID u64 | user name [64] | session key [24] |
"using hash" magic [3] | hash [4] = 0x80 bytes.
"""
import base64, json, secrets, struct, time

import rsa_pss

TICKET_MAGIC = 0xEFBDADDE
REQUEST_TASK = 46
REPLY_TASK = 47
TICKET_SIZE = 0x80


def user_id(online_id):
    """Account ID the runtime reports for an online ID (FNV-1a 64, ctrport/hle/online_config.cpp)."""
    value = 0xcbf29ce484222325
    for byte in online_id.encode():
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return value or 1


def client_ticket(title_id, online_id, session_key, now=None, lifetime=86400):
    now = int(time.time()) if now is None else now
    name = online_id.encode()[:63].ljust(64, b'\0')
    ticket = struct.pack('<IBIIIQQ', TICKET_MAGIC, 0, title_id, now, now + lifetime, 0,
                         user_id(online_id)) + name + session_key + b'\0' * 7
    assert len(ticket) == TICKET_SIZE
    return ticket


def server_ticket(online_id, session_key):
    """Project format, read back by the project's lobby: session key | user ID | user name."""
    ticket = session_key + struct.pack('<Q', user_id(online_id)) + online_id.encode()[:63].ljust(64, b'\0')
    return ticket.ljust(TICKET_SIZE, b'\0')


def reply(request, online_id, key):
    """Returns (status, headers, body bytes, session record) for an auth3 request body."""
    task = int(request.get('auth_task', 0))
    if task != REQUEST_TASK:
        return 400, {}, b'{"error":"unsupported auth_task"}', None
    session_key = secrets.token_bytes(24)
    title_id = int(request.get('title_id', 0))
    body = json.dumps({
        'auth_task': str(REPLY_TASK),
        'code': '700',
        'iv_seed': str(int(request.get('iv_seed', 0))),
        'client_ticket': base64.b64encode(client_ticket(title_id, online_id, session_key)).decode(),
        'server_ticket': base64.b64encode(server_ticket(online_id, session_key)).decode(),
        'client_id': '',
        'account_type': 'psn',
        'extra_data': json.dumps({'extended_data': '{}'}),
    }, separators=(',', ':')).encode()
    signature = base64.b64encode(rsa_pss.sign_pss_sha256(key, body)).decode()
    session = {'online_id': online_id, 'user_id': user_id(online_id), 'title_id': title_id,
               'session_key': session_key.hex()}
    return 200, {'X-Signature': signature}, body, session
