# SPDX-License-Identifier: GPL-2.0-or-later
"""Network-level check of the C++ project server's UDP relay (ctrport/online_server/relay.hpp).

Starts build/CTRPort-dev/ctr_online_server.exe on loopback (HTTP 18780, lobby TCP 3074,
relay UDP 13082), logs three synthetic players in with the retail wire formats (auth3, lobby
protocol 210, advertisement with a 67-byte address block), then exercises the relay as the
runtime does: POST /ctr/v1/relay-session, HELLO from a UDP socket, SEND to the address the
game knows a peer by, DELIVER on the peer's socket with the sender's advertised address.
Checks the refusals too (other room, unknown identity, withdrawn token, player gone).
The Python reference server has no relay, so this check runs the C++ server only.

    python -B tools/online/relay_check.py

Never contacts anything but 127.0.0.1.
"""
import base64, json, os, socket, struct, subprocess, sys, time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import e2e_server_check as e2e
import room
from demonware_tasks import STRING

ROOT = Path(__file__).resolve().parents[2]
RELAY = 13082
MAGIC = b'CTRR\x01'
HELLO, WELCOME, SEND, DELIVER, REFUSED = 1, 2, 3, 4, 5
REASONS = {1: 'not_registered', 2: 'unknown_destination', 3: 'destination_offline', 4: 'rate_limited', 5: 'malformed'}
results = []


def check(ok, what):
    results.append(bool(ok))
    print(json.dumps({'scope': 'relay_check', 'check': what, 'ok': bool(ok)}))


def address(ip, port):
    return socket.inet_aton(ip) + struct.pack('>H', port)


def parse(datagram):
    if len(datagram) < 6 or datagram[:5] != MAGIC:
        return ('foreign',)
    kind = datagram[5]
    ip, port = socket.inet_ntoa(datagram[6:10]), struct.unpack('>H', datagram[10:12])[0]
    if kind == REFUSED:
        return ('refused', '%s:%d' % (ip, port), REASONS.get(datagram[12], datagram[12]))
    if kind == WELCOME:
        return ('welcome', '%s:%d' % (ip, port), struct.unpack('>H', datagram[12:14])[0])
    if kind == DELIVER:
        return ('deliver', '%s:%d' % (ip, port), datagram[12:])
    return ('type', kind)


def exchange(sock, data):
    sock.sendto(data, (e2e.HOST, RELAY))
    try:
        return parse(sock.recvfrom(4096)[0])
    except socket.timeout:
        return ('timeout',)


def receive(sock):
    try:
        return parse(sock.recvfrom(4096)[0])
    except socket.timeout:
        return ('timeout',)


def scenario():
    password, other = room.token('relay password'), room.token('other password')
    names = {'RelayHost': (password, '198.51.100.20', 3174), 'RelayGuest': (password, '198.51.100.21', 3274),
             'RelayOther': (other, '198.51.100.22', 3374)}
    lobbies, keys = {}, {}
    for name, (selector, ip, port) in names.items():
        request = {'auth_task': '46', 'iv_seed': '1', 'title_id': '5760', 'identity': 'x', 'extra_data': '{}'}
        status, line, fields, body = e2e.http('POST', '/auth/', 'ctr-ps4-auth3.prod.demonware.net',
                                              {'X-CTR-Online-ID': name, 'X-CTR-Room-Token': selector,
                                               'Content-Type': 'application/json'}, json.dumps(request).encode())
        server_ticket = base64.b64decode(json.loads(body)['server_ticket'])
        lobbies[name] = e2e.Lobby(server_ticket[:24])
        block = bytearray(67)
        block[0:4], block[4:6] = socket.inet_aton('192.168.50.%d' % (port // 100)), struct.pack('<H', 3074)
        block[60:64], block[64:66], block[66] = socket.inet_aton(ip), struct.pack('<H', port), 2
        info = {'listen_server': {'local_address': base64.b64encode(bytes(block)).decode(),
                                  'security_id': base64.b64encode(bytes(8)).decode(),
                                  'security_key': base64.b64encode(bytes(16)).decode(), 'nat': 2},
                'dedicated_server': {'build_name': 'relay'}}
        lobbies[name].send_task(145, 2, e2e.typed((STRING, json.dumps(info))))
        lobbies[name].receive()
    check(all(l.result == 'connected' for l in lobbies.values()), 'three players logged in to the lobby')

    def session(name, selector=None):
        status, line, fields, body = e2e.http('POST', '/ctr/v1/relay-session', 'ctr.local',
                                              {'X-CTR-Online-ID': name, 'X-CTR-Room-Token': selector or names[name][0]})
        return status, json.loads(body)
    tokens = {}
    for name in names:
        status, reply = session(name)
        check(status == 200 and len(reply['token']) == 32 and reply['port'] == RELAY and reply['lease'] == 60,
              'relay session issued to ' + name)
        tokens[name] = bytes.fromhex(reply['token'])
    status, reply = session('RelayHost', other)
    check(status == 403 and reply['error'] == 'room mismatch', 'relay session refused for another room password')
    status, reply = session('RelayNobody', password)
    check(status == 403 and reply['error'] == 'not online in the lobby', 'relay session refused without a lobby connection')
    status, reply = session('RelayHost')  # re-issue: the earlier token is withdrawn
    old_token, tokens['RelayHost'] = tokens['RelayHost'], bytes.fromhex(reply['token'])

    sockets = {}
    for name in names:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.settimeout(2)
        s.bind((e2e.HOST, 0))
        sockets[name] = s
    host, guest, stranger = sockets['RelayHost'], sockets['RelayGuest'], sockets['RelayOther']
    check(exchange(host, MAGIC + bytes([HELLO]) + old_token) == ('refused', '0.0.0.0:0', 'not_registered'),
          'withdrawn token refused')
    for name, s in sockets.items():
        reply = exchange(s, MAGIC + bytes([HELLO]) + tokens[name])
        check(reply == ('welcome', '%s:%d' % s.getsockname(), 60), 'HELLO from %s answered with its endpoint' % name)

    host_addr, guest_addr = address('198.51.100.20', 3174), address('198.51.100.21', 3274)
    guest.sendto(MAGIC + bytes([SEND]) + host_addr + b'join request', (e2e.HOST, RELAY))
    check(receive(host) == ('deliver', '198.51.100.21:3274', b'join request'),
          'guest to host: delivered, guest shown by its advertised public endpoint')
    host.sendto(MAGIC + bytes([SEND]) + guest_addr + b'welcome guest', (e2e.HOST, RELAY))
    check(receive(guest) == ('deliver', '198.51.100.20:3174', b'welcome guest'),
          'host to guest: delivered, host shown by the address the guest used')
    burst = 50
    for i in range(burst):
        guest.sendto(MAGIC + bytes([SEND]) + host_addr + struct.pack('>I', i) + bytes(150), (e2e.HOST, RELAY))
    got = [receive(host) for _ in range(burst)]
    check(all(g[0] == 'deliver' and len(g[2]) == 154 for g in got), '%d game-sized datagrams relayed in a burst' % burst)
    check(exchange(stranger, MAGIC + bytes([SEND]) + host_addr + b'x') == ('refused', '198.51.100.20:3174', 'unknown_destination'),
          'player of another room cannot reach the host')
    outsider = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    outsider.settimeout(2)
    outsider.bind((e2e.HOST, 0))
    check(exchange(outsider, MAGIC + bytes([SEND]) + host_addr + b'x') == ('refused', '198.51.100.20:3174', 'not_registered'),
          'endpoint without HELLO refused')
    outsider.sendto(b'\x20\x02\x00\x00', (e2e.HOST, RELAY))
    check(receive(outsider) == ('timeout',), 'foreign datagram ignored without answer')
    outsider.close()

    lobbies['RelayGuest'].close()
    time.sleep(0.5)
    check(exchange(host, MAGIC + bytes([SEND]) + guest_addr + b'x') == ('refused', '198.51.100.21:3274', 'unknown_destination'),
          'guest who left the lobby is no longer reachable')
    check(exchange(guest, MAGIC + bytes([SEND]) + host_addr + b'x') == ('refused', '198.51.100.20:3174', 'not_registered'),
          'guest who left the lobby can no longer send')
    for s in sockets.values():
        s.close()
    for name, lobby in lobbies.items():
        if name != 'RelayGuest':
            lobby.close()


def main():
    exe = ROOT / 'build/CTRPort-dev/ctr_online_server.exe'
    args = [str(exe), '--host', e2e.HOST, '--http', str(e2e.HTTP), '--tcp', '', '--udp', '',
            '--public-host', e2e.PUBLIC, '--discovery-port', str(e2e.DISCOVERY), '--relay-port', str(RELAY)]
    process = subprocess.Popen(args, cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        deadline = time.time() + 20
        while time.time() < deadline:
            try:
                socket.create_connection((e2e.HOST, e2e.HTTP), timeout=1).close()
                break
            except OSError:
                time.sleep(0.2)
        scenario()
    finally:
        process.terminate()
        try:
            process.wait(10)
        except subprocess.TimeoutExpired:
            process.kill()
    passed = all(results)
    print(json.dumps({'scope': 'relay_check', 'checks': len(results), 'passed': passed}))
    return 0 if passed else 1


if __name__ == '__main__':
    sys.exit(main())
