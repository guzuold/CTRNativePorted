# SPDX-License-Identifier: GPL-2.0-or-later
"""Network-level check of a project server: Python reference or C++ port.

Starts the server under test on loopback (HTTP 18780, lobby TCP 3074, discovery UDP 13078..13081,
public address 192.0.2.10 from TEST-NET-1), plays three synthetic players through it with the
wire formats of the retail client (auth3, Umbrella, lobby protocol 210 records, typed tasks,
matchmaking, lobby documents, peer authorization, Web API routes, discovery), and writes a
normalized transcript: random values (keys, tokens, IDs, transaction numbers, times) become
symbols, so the transcripts of the two servers can be compared line by line.

    python -B tools/online/e2e_server_check.py --server python  > a.json
    python -B tools/online/e2e_server_check.py --server cpp     > b.json
    python -B tools/online/e2e_server_check.py --compare        (runs both and diffs)

The X-Signature of each auth3 reply is verified with OpenSSL (RSA-PSS, SHA-256, salt 0) against
the key served at /ctr/v1/traffic-key. Never contacts anything but 127.0.0.1.
"""
import argparse, base64, hashlib, hmac, json, os, shutil, socket, struct, subprocess, sys, tempfile, time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import aes128, discovery, room
from demonware_tasks import Reader, Writer, STRING, UINT64

ROOT = Path(__file__).resolve().parents[2]
HOST, HTTP, DISCOVERY, PUBLIC = '127.0.0.1', 18780, 13078, '192.0.2.10'
SERVERS = {
    'python': [sys.executable, '-B', str(ROOT / 'tools/online/ctr_online_server.py')],
    'cpp': [str(Path(os.environ.get('CTR_TEST_SERVER_EXE', ROOT / 'build/CTRPort-claude/ctr_online_server.exe')).resolve())],
}
ARGS = ['--host', HOST, '--http', str(HTTP), '--tcp', '', '--udp', '', '--public-host', PUBLIC,
        '--discovery-port', str(DISCOVERY)]


class Symbols:
    """Random values to stable names in order of appearance."""
    def __init__(self):
        self.names = {}

    def __call__(self, kind, value):
        key = (kind, value)
        if key not in self.names:
            self.names[key] = '%s#%d' % (kind, sum(1 for k in self.names if k[0] == kind) + 1)
        return self.names[key]


sym = Symbols()
transcript = []


def note(step, value):
    def plain(v):
        if isinstance(v, bytes):
            return v.decode('utf-8', 'replace')
        if isinstance(v, (list, tuple)):
            return [plain(x) for x in v]
        return v
    transcript.append([step, plain(value)])


def http(method, path, host='ctr.local', headers=None, body=b''):
    s = socket.create_connection((HOST, HTTP), timeout=10)
    head = '%s %s HTTP/1.1\r\nHost: %s\r\nContent-Length: %d\r\n' % (method, path, host, len(body))
    for name, value in (headers or {}).items():
        head += '%s: %s\r\n' % (name, value)
    s.sendall(head.encode('latin-1') + b'\r\n' + body)
    raw = b''
    while True:
        chunk = s.recv(65536)
        if not chunk:
            break
        raw += chunk
    s.close()
    head, _, data = raw.partition(b'\r\n\r\n')
    lines = head.decode('latin-1').split('\r\n')
    status = int(lines[0].split(' ')[1])
    fields = {}
    for line in lines[1:]:
        name, _, value = line.partition(':')
        fields[name.strip()] = value.strip()
    return status, lines[0], fields, data


def normalize(value, now):
    """Times near now -> TIME, large integers -> symbols; recursive over JSON."""
    if isinstance(value, bool) or value is None:
        return value
    if isinstance(value, int):
        if abs(value - now) < 3 * 86400:
            return 'TIME'
        if abs(value - now - 86400) < 3 * 86400:
            return 'TIME+1d'
        return sym('n', value) if value > 0xffff else value
    if isinstance(value, list):
        return [normalize(v, now) for v in value]
    if isinstance(value, dict):
        # ctr_migration: host-migration routes, a C++-only extension of the lobby document.
        return {k if not k.isdigit() else sym('n', int(k)): normalize(v, now) for k, v in value.items()
                if k != 'ctr_migration'}
    if isinstance(value, str):
        if value.startswith('ctr.') and len(value) == 47:
            return 'ACCESS_TOKEN'
        if len(value) == 32 and all(c in '0123456789abcdef' for c in value):
            return 'HEX32'
        return value
    return value


def verify_signature(key_der, body, signature):
    openssl = shutil.which('openssl')
    if not openssl:
        return 'openssl missing'
    with tempfile.TemporaryDirectory() as directory:
        d = Path(directory)
        (d / 'key.der').write_bytes(key_der)
        (d / 'body').write_bytes(body)
        (d / 'sig').write_bytes(base64.b64decode(signature))
        subprocess.run([openssl, 'pkey', '-pubin', '-inform', 'DER', '-in', d / 'key.der', '-out', d / 'key.pem'],
                       check=True, capture_output=True)
        result = subprocess.run([openssl, 'dgst', '-sha256', '-verify', d / 'key.pem', '-sigopt', 'rsa_padding_mode:pss',
                                 '-sigopt', 'rsa_pss_saltlen:0', '-signature', d / 'sig', d / 'body'],
                                capture_output=True, text=True)
        return 'verified' if result.returncode == 0 and 'Verified OK' in result.stdout else 'BAD SIGNATURE'


def hkdf_expand(prk, info, length):
    out, block, counter = b'', b'', 1
    while len(out) < length:
        block = hmac.new(prk, block + info + bytes([counter]), hashlib.sha1).digest()
        out += block
        counter += 1
    return out[:length]


class Lobby:
    """Client side of lobby protocol 210, as the retail client speaks it."""
    def __init__(self, session_key):
        self.s = socket.create_connection((HOST, 3074), timeout=10)
        nonce = os.urandom(8)
        self.s.sendall(struct.pack('<5I', 200, 200, 0xd2, 0xdc, 0x10000) + nonce)
        size = struct.unpack('<I', self.exact(4))[0]
        header = struct.pack('<I', size) + self.exact(size)
        assert header[4:6] == b'\xab\x81', header.hex()
        body = b'\xab\x82' + os.urandom(48) + bytes(8)
        auth = struct.pack('<I', len(body)) + body
        material = struct.pack('<III', 0xd2, 0xdc, 0x10000) + nonce + header + auth[:-8]
        prk = hmac.new(hashlib.sha1(material).digest(), session_key, hashlib.sha1).digest()
        challenge = hkdf_expand(prk, b'CLIENTCHAL', 16)
        self.s.sendall(auth[:-8] + challenge[:8])
        size = struct.unpack('<I', self.exact(4))[0]
        done = self.exact(size)
        self.result = 'connected' if done == b'\xab\x83' + challenge[8:] else 'refused ' + done[:2].hex()
        keys = hkdf_expand(prk, b'BDDATA', 72)
        self.client_mac, self.server_mac, self.client_aes, self.server_aes = keys[:20], keys[20:40], keys[40:56], keys[56:72]
        self.counter = 1

    def exact(self, n):
        data = b''
        while len(data) < n:
            chunk = self.s.recv(n - len(data))
            if not chunk:
                raise ConnectionError('closed')
            data += chunk
        return data

    def send_task(self, service, task, params=b''):
        payload = bytes([service]) + bytes([3, task]) + params
        size = (len(payload) + 0x14) & ~0xf
        plain = struct.pack('<IB', len(payload), 0x86) + payload
        plain += bytes(size - len(plain))
        iv = os.urandom(16)
        body = b'\xab\x85' + struct.pack('<I', self.counter) + iv + aes128.cbc_encrypt(self.client_aes, iv, plain)
        self.counter += 1
        head = struct.pack('<I', len(body) + 8) + body
        self.s.sendall(head + hmac.new(self.client_mac, head, hashlib.sha1).digest()[:8])

    def keepalive(self):
        self.s.sendall(bytes(4))
        return self.exact(4).hex()

    def receive(self):
        size = struct.unpack('<I', self.exact(4))[0]
        data = self.exact(size)
        whole = struct.pack('<I', size) + data
        assert hmac.new(self.server_mac, whole[:-8], hashlib.sha1).digest()[:8] == whole[-8:], 'server MAC'
        plain = aes128.cbc_decrypt(self.server_aes, data[6:22], data[22:-8])
        length, kind = struct.unpack_from('<IB', plain)
        return kind, plain[5:5 + length]

    def close(self):
        self.s.close()


def task_values(kind, payload, now):
    values = Reader(payload).values()
    if kind == 1 and values:
        values[0] = (values[0][0], 'TXID')  # transaction counter
    out = []
    for t, v in values:
        if t == STRING and v.startswith('{'):
            v = normalize(json.loads(v), now)
        elif isinstance(v, int):
            v = normalize(v, now)
        elif isinstance(v, str) and t == 19:
            v = 'BLOB(%d)' % (len(v) // 2)
        out.append([t, v])
    return [kind, out]


def typed(*values):
    w = Writer()
    for kind, value in values:
        if kind == STRING:
            w.string(value)
        elif kind == UINT64:
            w.u64(value)
        elif kind == 8:
            w.u32(value)
        elif kind == 3:
            w.u8(value)
        elif kind == 9:
            w.data += bytes([9]) + struct.pack('<q', value)
        elif kind == 19:
            w.blob(value)
    return bytes(w.data)


def scenario():
    now = int(time.time())
    status, line, fields, key = http('GET', '/ctr/v1/traffic-key')
    note('traffic_key', [line, fields.get('Content-Type'), len(key)])
    status, line, fields, data = http('GET', '/ctr/v1/config')
    note('config', [line, json.loads(data)])

    players = {}
    password, other = room.token('same password'), room.token('other password')
    for name, selector in [('E2EAlpha', password), ('E2EBravo', password), ('E2ECharlie', other)]:
        request = {'auth_task': '46', 'iv_seed': '1588948653', 'title_id': '5760', 'identity': 'x',
                   'extra_data': '{}'}
        status, line, fields, body = http('POST', '/auth/', 'ctr-ps4-auth3.prod.demonware.net',
                                          {'X-CTR-Online-ID': name, 'X-CTR-Room-Token': selector,
                                           'Content-Type': 'application/json'}, json.dumps(request).encode())
        reply = json.loads(body)
        client = base64.b64decode(reply['client_ticket'])
        server = base64.b64decode(reply['server_ticket'])
        magic, kind, title, issued, expires, license_id, user = struct.unpack_from('<IBIIIQQ', client)
        note('auth3 ' + name, [line, sorted(fields), verify_signature(key, body, fields['X-Signature']),
                               list(reply), reply['auth_task'], reply['code'], reply['iv_seed'], reply['extra_data'],
                               hex(magic), title, expires - issued, sym('user', user), client[0x21:0x61].rstrip(b'\0').decode(),
                               client[0x61:0x79] == server[:24], server[24:32] == struct.pack('<Q', user)])
        players[name] = {'key': server[:24], 'server_ticket': server, 'user': user, 'selector': selector}
    request = json.dumps({'auth_task': '46'}).encode()
    # Without a room token the player is admitted to public matchmaking (the reply holds random tickets).
    status, line, fields, body = http('POST', '/auth/', 'a-auth3.b', {'X-CTR-Online-ID': 'x'}, request)
    note('auth3 without room', [line, sorted(json.loads(body))])
    note('auth3 bad json', http('POST', '/auth/', 'a-auth3.b', {'X-CTR-Room-Token': password}, b'{')[1:4:2])
    note('auth3 other task', http('POST', '/auth/', 'a-auth3.b', {'X-CTR-Room-Token': password},
                                  json.dumps({'auth_task': '12'}).encode())[1:4:2])

    ticket = base64.b64encode(players['E2EAlpha']['server_ticket']).decode()
    status, line, fields, body = http('POST', '/v1.0/tokens/lsg/', 'prod.umbrella.demonware.net', {},
                                      json.dumps({'ticket': ticket}).encode())
    note('umbrella', [line, normalize(json.loads(body), now)])
    note('umbrella unknown', http('POST', '/v1.0/tokens/lsg/', 'prod.umbrella.demonware.net', {},
                                  json.dumps({'ticket': base64.b64encode(bytes(128)).decode()}).encode())[1:4:2])

    lobbies = {}
    for name in players:
        lobbies[name] = Lobby(players[name]['key'])
        note('lobby ' + name, lobbies[name].result)
    try:
        intruder = Lobby(os.urandom(24))
        note('lobby unknown key', intruder.result)
    except (ConnectionError, struct.error, AssertionError) as error:
        note('lobby unknown key', 'error ' + type(error).__name__)
    try:
        twin = Lobby(players['E2EAlpha']['key'])
        note('lobby identity in use', twin.result)
    except ConnectionError as error:
        note('lobby identity in use', 'closed')

    a, b, c = lobbies['E2EAlpha'], lobbies['E2EBravo'], lobbies['E2ECharlie']
    note('keepalive', a.keepalive())
    for service, task, params, label in [(12, 6, b'', 'utc time'), (38, 6, typed((STRING, 'report')), 'report'),
                                         (99, 1, b'', 'unserved'),
                                         (4, 1, typed((8, 7), (UINT64, players['E2EAlpha']['user']), (3, 0), (9, 1234)), 'stats write'),
                                         (4, 1, typed((8, 7), (UINT64, 1), (3, 0), (9, 1)), 'stats other account')]:
        a.send_task(service, task, params)
        note('task ' + label, task_values(*a.receive(), now))
    # REST over LSG: the retail A/B request envelope.
    fields_pb = b''
    def blob(tag, value):
        return bytes([tag << 3 | 2, len(value)]) + value
    fields_pb = blob(1, b'POST') + blob(2, b'http://abtesting/v2/users/psn-42/enrollments/') + blob(4, blob(1, b'Accept') + blob(2, b'*/*'))
    a.send_task(194, 1, bytes([23, 8]) + struct.pack('<I', len(fields_pb)) + fields_pb + b'\0')
    kind, payload = a.receive()
    note('task rest', [kind, payload[:1].hex(), payload[9:].hex()])

    addresses = {'E2EAlpha': '198.51.100.10', 'E2EBravo': '198.51.100.11', 'E2ECharlie': '198.51.100.12'}
    for name, lobby in lobbies.items():
        block = bytearray(67)
        block[60:64] = socket.inet_aton(addresses[name])
        info = {'listen_server': {'local_address': base64.b64encode(bytes(block)).decode(),
                                  'security_id': base64.b64encode(bytes(8)).decode(),
                                  'security_key': base64.b64encode(bytes(16)).decode(), 'nat': 2},
                'dedicated_server': {'build_name': 'e2e'}}
        lobby.send_task(145, 2, typed((STRING, json.dumps(info))))
        note('advertise ' + name, task_values(*lobby.receive(), now))
        lobby.send_task(145, 3)
        kind, payload = lobby.receive()
        players[name]['token'] = Reader(payload).values()[-1][1]
        note('player token ' + name, task_values(kind, payload, now))
    a.send_task(145, 2, typed((STRING, '{"listen_server":{}}')))
    note('advertise bad schema', task_values(*a.receive(), now))
    a.send_task(138, 1, typed((STRING, 'ctx'), (19, bytes(67)), (8, 1), (8, 2), (19, b'c')))
    kind, payload = a.receive()
    session = bytes.fromhex(Reader(payload).values()[-1][1])
    note('register session', task_values(kind, payload, now))
    a.send_task(138, 12, typed((STRING, 'ctx'), (19, session), (8, 1), (19, bytes(67)), (8, 1), (8, 2), (19, b'c')))
    note('session players', task_values(*a.receive(), now))

    def allowed(asker, ip, selector=None):
        status, line, fields, body = http('GET', '/ctr/v1/peer-allowed?ip=' + ip, 'ctr.local',
                                          {'X-CTR-Online-ID': asker, 'X-CTR-Room-Token': selector or players.get(asker, {}).get('selector', password)})
        return [line, json.loads(body)]
    note('peer same room', allowed('E2EAlpha', addresses['E2EBravo']))
    note('peer other room', allowed('E2EAlpha', addresses['E2ECharlie']))
    note('peer stranger', allowed('E2EAlpha', '203.0.113.99'))
    note('peer requester offline', allowed('E2EDelta', addresses['E2EBravo'], password))
    note('peer lobby source', allowed('E2EBravo', PUBLIC))

    def search(name):
        account = players[name]['user']
        return {'members': [account], 'tokens': [players[name]['token']], 'lobby_slots': {'min': 2, 'max': 8},
                'dedicated_server': {'request_level': 'not_needed'}, 'team_balance': {'num_teams': 0},
                'ruleset_payload': {'filter': {'matchmaking_version': 200, 'playlist_id': 0}},
                'player_id_to_slots': {str(account): 0}, 'performance': {'game_type': 1}}
    a.send_task(145, 18, typed((STRING, json.dumps(search('E2EAlpha')))))
    note('search alpha', task_values(*a.receive(), now))
    c.send_task(145, 18, typed((STRING, json.dumps(search('E2ECharlie')))))
    note('search charlie', task_values(*c.receive(), now))
    b.send_task(145, 18, typed((STRING, json.dumps(search('E2EBravo')))))
    note('search bravo', task_values(*b.receive(), now))
    kind, payload = a.receive()
    created = Reader(payload).values()
    note('push to host', task_values(kind, payload, now))
    lobby_id, host_doc = created[2][1], json.loads(created[4][1])
    host_doc['player_state'][str(players['E2EBravo']['user'])] = 1
    host_doc['attachment'] = ''
    a.send_task(145, 15, typed((UINT64, lobby_id), (UINT64, 1), (STRING, json.dumps(host_doc))))
    note('sync host document', task_values(*a.receive(), now))
    note('join pushed to guest', task_values(*b.receive(), now))
    note('update pushed to host', task_values(*a.receive(), now))
    b.send_task(145, 15, typed((UINT64, lobby_id), (UINT64, 2), (STRING, json.dumps(host_doc))))
    note('guest sync refused', task_values(*b.receive(), now))

    friends = '/webapi/sdk:userProfile/v1/users/%d/friendList' % players['E2EAlpha']['user']
    note('friends', http('GET', friends + '?offset=0&limit=100', 'ctr.local', {'X-CTR-Online-ID': 'E2EAlpha'})[1:4:2])
    note('friends other', http('GET', friends, 'ctr.local', {'X-CTR-Online-ID': 'E2EBravo'})[1:4:2])
    note('friends bad limit', http('GET', friends + '?limit=0', 'ctr.local', {'X-CTR-Online-ID': 'E2EAlpha'})[1:4:2])
    meta = json.dumps({'sessionPrivacy': 'private', 'sessionMaxUser': 8, 'sessionName': 'e2e', 'availablePlatforms': ['PS4']}).encode()
    body = (b'--XB\r\nContent-Type: application/json\r\nContent-Description: session-request\r\nContent-Length: %d\r\n\r\n' % len(meta) +
            meta + b'\r\n--XB\r\nContent-Type: application/octet-stream\r\nContent-Description: changeable-session-data\r\n'
            b'Content-Length: 3\r\n\r\nabc')
    status, line, fields, data = http('POST', '/webapi/sdk:sessionInvitation/v1/sessions', 'ctr.local',
                                      {'X-CTR-Online-ID': 'E2EAlpha', 'Content-Type': 'multipart/mixed; boundary="XB"'}, body)
    note('session create', [line, normalize(json.loads(data), now)])
    note('session bad', http('POST', '/webapi/sdk:sessionInvitation/v1/sessions', 'ctr.local',
                             {'X-CTR-Online-ID': 'E2EAlpha', 'Content-Type': 'multipart/mixed; boundary=XB'}, b'--XB\r\nbad')[1:4:2])
    note('webapi other', http('GET', '/webapi/sdk:other/v1/x', 'ctr.local')[1:4:2])
    note('not served', http('GET', '/nothing')[1:4:2])

    u = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    u.settimeout(3)
    u.bind((HOST, 0))
    for request in (bytes([30, 3, 0]), bytes([20, 2, 0, 0]), bytes([20, 2, 0, 1]), bytes([20, 2, 0, 2]), bytes([20, 2, 0, 3])):
        u.sendto(request, (HOST, DISCOVERY))
        try:
            reply, source = u.recvfrom(2048)
            seen = reply[3:9]
            # IP replies repeat the observed endpoint in the protected form (depends on our port).
            tail = discovery.address((PUBLIC, u.getsockname()[1]), True) == reply[9:] if request[0] == 30 else reply[9:].hex()
            note('discovery ' + request.hex(), [source[1] - DISCOVERY, reply[:3].hex(), socket.inet_ntoa(seen[:4]),
                                                struct.unpack('<H', seen[4:6])[0] == u.getsockname()[1], tail])
        except socket.timeout:
            note('discovery ' + request.hex(), 'timeout')
    u.close()

    b.close()
    time.sleep(0.5)
    note('peer after guest left', allowed('E2EAlpha', addresses['E2EBravo']))
    a.close()
    c.close()


def run(server):
    process = subprocess.Popen(SERVERS[server] + ARGS, cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        deadline = time.time() + 20
        while time.time() < deadline:
            try:
                socket.create_connection((HOST, HTTP), timeout=1).close()
                break
            except OSError:
                time.sleep(0.2)
        global sym, transcript
        sym, transcript = Symbols(), []
        scenario()
        return transcript
    finally:
        process.terminate()
        try:
            process.wait(10)
        except subprocess.TimeoutExpired:
            process.kill()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--server', choices=sorted(SERVERS))
    parser.add_argument('--compare', action='store_true')
    args = parser.parse_args()
    if not args.compare:
        print(json.dumps(run(args.server), indent=1))
        return 0
    results = {name: run(name) for name in ('python', 'cpp')}
    differences = 0
    for left, right in zip(results['python'], results['cpp']):
        same = left == right
        differences += not same
        print(json.dumps({'step': left[0], 'same': same, **({} if same else {'python': left[1], 'cpp': right[1]})}))
    if len(results['python']) != len(results['cpp']):
        differences += 1
        print(json.dumps({'step': 'length', 'python': len(results['python']), 'cpp': len(results['cpp'])}))
    print(json.dumps({'scope': 'e2e_server_check', 'steps': len(results['python']), 'differences': differences}))
    return 1 if differences else 0


if __name__ == '__main__':
    sys.exit(main())
