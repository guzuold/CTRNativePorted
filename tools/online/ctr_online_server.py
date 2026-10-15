# SPDX-License-Identifier: GPL-2.0-or-later
"""CTR project server for R8: answers in place of Demonware and PSN web services.

The runtime with CTR_ONLINE=1 resolves every *.demonware.net name to this server and sends
it all HTTP traffic in plain HTTP on one port (ctrport/hle/online_http.h). It never connects
anywhere itself.

Served so far:
- GET  /ctr/v1/traffic-key: this server's RSA-2048 public key (DER), which the runtime puts in
  place of the auth3 traffic key in the eboot (ctrport/online_traffic_key.hpp);
- POST /auth/ on host *-auth3.*.demonware.net: auth3 login (demonware_auth3.py);
- POST /v1.0/tokens/lsg/ on host *.umbrella.*: Umbrella account and access token;
- TCP 3074: Demonware lobby, protocol 210 handshake with the auth3 session key
  (demonware_lobby.py); HTTP proxy tasks, UTC time, persistent statistics writes,
  explicit errors for other tasks and zero-frame keepalive replies;
- UDP discovery: real source endpoints on --peer-host and --stun-secondary-host,
  ports 3074 and --stun-alternate-port; only the observed IP/NAT request versions;
- /webapi/sdk:userProfile/v1/users/<account>/friendList: project friend graph;
- /webapi/sdk:entitlement/v1/users/me/entitlements: empty service entitlement list;
  other Web API routes receive 404.
Other UDP (including QoS) is recorded; other HTTP receives an explicit 404.

Internet play (--public-host <public IPv4>): discovery runs on one address with four ports
(--discovery-port, +1, +2, +3 by default 3078..3081), announced to the runtime with
GET /ctr/v1/config; a game on this same PC (loopback) is reported with the public address;
GET /ctr/v1/peer-allowed?ip=X tells a runtime whether X belongs to a player connected in its
room (same password), the only public addresses the runtime will send to. Ports to forward to
this PC: TCP 8780 and 3074, UDP 3078-3081, and UDP 3074 for the game itself.

Usage (from the project root):
    python -B tools/online/ctr_online_server.py [--host 0.0.0.0] [--http 8780] [--peer-host 127.0.0.2]
    python -B tools/online/ctr_online_server.py --public-host 203.0.113.7   (internet)
On the same PC as the game without --public-host, the server's UDP 3074 needs its own loopback
address: start the game with --online-server 127.0.0.2. State: runtime/user/online/ (server key). Log:
runtime/user/logs/online_server_<time>.jsonl, one JSON object per event.
"""
import argparse, asyncio, base64, datetime, json, re, secrets, socket, struct, sys, time
from urllib.parse import parse_qs
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import demonware_auth3, demonware_lobby, demonware_rest, demonware_tasks, rsa_pss, local_stats, local_marketplace, local_achievements, local_friends, local_matchmaking, local_sessions, discovery, room, lobby_documents

root = Path(__file__).resolve().parents[2]
state = root / 'runtime/user/online'
log_path = root / 'runtime/user/logs' / (
    'online_server_' + datetime.datetime.now().strftime('%Y%m%d_%H%M%S') + '.jsonl')
log_file = None
server_key = None
sessions = {}
friends_store = None
session_store = None
live_clients = {}
pending_lobbies = {}


async def send_lobby(client, payload, payload_type):
    async with client['send_lock']:
        client['writer'].write(client['crypto'].seal_record(payload,payload_type))
        await client['writer'].drain()


async def create_ready_lobbies():
    available={account:client['user']['room_token'] for account,client in live_clients.items()}
    for plan in demonware_tasks.matchmaking_store.match_ready(available):
        host=plan['host']
        row=demonware_tasks.matchmaking_store.database.execute(
            'SELECT document FROM player_info WHERE account=?',(str(host),)).fetchone()
        host_doc,backend_doc=lobby_documents.documents(plan,json.loads(row[0]))
        demonware_tasks.matchmaking_store.save_documents(plan['lobby_id'],host_doc,backend_doc,1)
        pending_lobbies[plan['lobby_id']]=plan
        player=next(p for p in plan['players'] if p['account']==host)
        await send_lobby(live_clients[host],lobby_documents.create_event(player['search_id'],plan['lobby_id'],host_doc,backend_doc),lobby_documents.PUSH_RECORD)
        record(event='room_host_notified',lobby=plan['lobby_id'],host=host,
               members=[p['account'] for p in plan['players']])


async def publish_host_document(lobby):
    plan=pending_lobbies.get(lobby)
    if not plan:
        return
    row=demonware_tasks.matchmaking_store.database.execute('SELECT host_document,backend_document '
                                                         'FROM lobby_documents WHERE lobby=?',(str(lobby),)).fetchone()
    host,backend=[json.loads(value) for value in row]
    notified=set(plan.get('guests_notified',[]))
    for player in plan['players']:
        client=live_clients.get(player['account'])
        if not client or client['user']['room_token'] != plan['selector']:
            continue
        if player['account']==plan['host'] or player['account'] in notified:
            await send_lobby(client,lobby_documents.update_event(lobby,host,backend),lobby_documents.PUSH_RECORD)
        else:
            await send_lobby(client,lobby_documents.join_event(player['search_id'],lobby,host,backend),lobby_documents.PUSH_RECORD)
            notified.add(player['account'])
            record(event='room_guest_notified',lobby=lobby,account=player['account'],host=plan['host'])
    plan['guests_notified']=list(notified)
    record(event='room_host_document_synced',lobby=lobby,version=host['update_id'])


def record(**event):
    event = {'time': round(time.time(), 3), **event}
    log_file.write(json.dumps(event) + '\n')
    log_file.flush()
    summary = {k: (v[:96] + '...' if isinstance(v, str) and len(v) > 96 else v)
               for k, v in event.items()}
    print(json.dumps(summary), flush=True)


def load_key():
    """The server's traffic key, created on first start and kept in runtime/user/online."""
    path = state / 'server_key.json'
    if path.exists():
        key = {k: int(v) for k, v in json.loads(path.read_text()).items()}
    else:
        state.mkdir(parents=True, exist_ok=True)
        key = rsa_pss.generate()
        path.write_text(json.dumps({k: str(v) for k, v in key.items()}))
    (state / 'server_key.der').write_bytes(rsa_pss.spki_der(key['n'], key['e']))
    return key


# PSN Web API routes ((method, path prefix) -> (status, JSON body)); others get 404.
WEBAPI_ROUTES = {}


# Internet settings, filled by main(): public address and announced discovery port.
internet = {'public_host': None, 'discovery_port': 3074}


def reachable_address(ip):
    """Address other players use for a client seen from `ip` (loopback -> public address)."""
    if internet['public_host'] and ip.startswith('127.'):
        return internet['public_host']
    return ip


def room_addresses(room_token):
    """Addresses of the players connected to the lobby in a room: their lobby connection
    source and the public address in their own advertisement (bytes 60..63 of the address
    block, the endpoint the discovery service observed)."""
    found = set()
    for account, client in live_clients.items():
        if client['user'].get('room_token') != room_token:
            continue
        if client.get('ip'):
            found.add(client['ip'])
        row = demonware_tasks.matchmaking_store.database.execute(
            'SELECT document FROM player_info WHERE account=?', (str(account),)).fetchone()
        if row:
            try:
                block = base64.b64decode(json.loads(row[0])['listen_server']['local_address'])
                found.add('%d.%d.%d.%d' % tuple(block[60:64]))
            except (ValueError, KeyError, IndexError, TypeError):
                pass
    return found


def http_answer(method, target, headers, body):
    """Returns (status, extra headers, body bytes, event name, extra event fields)."""
    host = headers.get('host', '').split(':')[0].lower()
    online_id = headers.get('x-ctr-online-id', '')
    if method == 'GET' and target == '/ctr/v1/config':
        payload = {'discovery_port': internet['discovery_port'], 'public_host': internet['public_host']}
        return 200, {'Content-Type': 'application/json'}, json.dumps(payload).encode(), 'config', {}
    if method == 'GET' and target.startswith('/ctr/v1/peer-allowed?'):
        ip = parse_qs(target.partition('?')[2]).get('ip', [''])[0]
        room_token = headers.get('x-ctr-room-token', '')
        requester = live_clients.get(demonware_auth3.user_id(online_id)) if online_id else None
        allowed = bool(requester and room.valid_token(room_token) and
                       requester['user'].get('room_token') == room_token and
                       ip in room_addresses(room_token))
        return (200 if allowed else 403), {'Content-Type': 'application/json'}, \
            json.dumps({'allowed': allowed}).encode(), 'peer_allowed', {'ip': ip, 'allowed': allowed}
    if method == 'GET' and target == '/ctr/v1/traffic-key':
        return 200, {'Content-Type': 'application/octet-stream'}, \
            rsa_pss.spki_der(server_key['n'], server_key['e']), 'traffic_key', {}
    if method == 'POST' and target.startswith('/auth/') and '-auth3.' in host:
        room_token = headers.get('x-ctr-room-token','')
        if not room.valid_token(room_token):
            return 400, {}, b'{"error":"project room password required"}', 'auth3_room_rejected', {}
        try:
            request = json.loads(body)
        except ValueError:
            return 400, {}, b'{"error":"bad json"}', 'auth3', {}
        status, extra, data, session = demonware_auth3.reply(request, online_id, server_key)
        if session:
            session['room_token'] = room_token
            sessions[bytes.fromhex(session['session_key'])] = session
        return status, {'Content-Type': 'application/json', **extra}, data, 'auth3', \
            {'request': request, 'reply': data.decode(), 'session_user_id': session and session['user_id']}
    if method == 'POST' and target.startswith('/v1.0/tokens/lsg/') and '.umbrella.' in host:
        # Umbrella login reply as the client reads it (retail v01.21): umbrellaID, accessToken,
        # expires, accounts [{provider, username, accountID, authorized}].
        try:
            request = json.loads(body)
            ticket = base64.b64decode(request['ticket'])
            user = next((s for k, s in sessions.items() if ticket[:24] == k), None)
        except (ValueError, KeyError):
            user = None
        if not user:
            return 401, {'Content-Type': 'application/json'}, b'{"error":"unknown ticket"}', 'umbrella', {}
        now = int(time.time())
        payload = {'umbrellaID': user['user_id'],
                   'accessToken': 'ctr.' + secrets.token_urlsafe(32), 'expires': now + 86400,
                   'accounts': [{'provider': 'psn', 'username': user['online_id'],
                                 'accountID': user['user_id'], 'authorized': True}]}
        data = json.dumps(payload).encode()
        return 200, {'Content-Type': 'application/json'}, data, 'umbrella', {'reply': payload}
    if target.startswith('/webapi/'):
        path = target.removeprefix('/webapi/')
        if method == 'POST' and path == 'sdk:sessionInvitation/v1/sessions':
            if not online_id:
                return 403, {}, b'{"error":{"message":"project identity required"}}', 'webapi_session_create', {}
            try:
                account = demonware_auth3.user_id(online_id)
                payload = session_store.create(account, headers.get('content-type',''), body)
            except (ValueError, KeyError, UnicodeError) as error:
                capture = log_path.with_suffix('.session_rejected.bin')
                if len(body) <= 2*1024*1024:
                    capture.write_bytes(body)
                return 400, {}, b'{"error":{"message":"invalid project session"}}', 'webapi_session_create', {'reason':str(error),'content_type':headers.get('content-type',''),'body_length':len(body),'capture':str(capture)}
            return 201, {'Content-Type':'application/json'}, json.dumps(payload).encode(), 'webapi_session_create', {'account':account,'reply':payload}
        # The API group contains a colon: split the query without interpreting a scheme.
        friend_route = re.fullmatch(r'sdk:userProfile/v1/users/([0-9]+)/friendList', path.split('?', 1)[0])
        if method == 'GET' and friend_route:
            account = int(friend_route[1])
            if not online_id or account != demonware_auth3.user_id(online_id):
                return 403, {}, b'{"error":{"message":"project identity mismatch"}}', 'webapi_friends', {}
            try:
                query = parse_qs(path.partition('?')[2])
                payload = friends_store.page(account, int(query.get('offset', ['0'])[0]),
                                            int(query.get('limit', ['500'])[0]))
            except (ValueError, AttributeError):
                return 400, {}, b'{"error":{"message":"invalid friend request"}}', 'webapi_friends', {}
            except NotImplementedError:
                return 501, {}, b'{"error":{"message":"nonempty friend schema not supported"}}', 'webapi_friends', {}
            return 200, {'Content-Type': 'application/json'}, json.dumps(payload).encode(), 'webapi_friends', {'account': account, 'reply': payload}
        if method == 'GET' and path.split('?', 1)[0] == 'sdk:entitlement/v1/users/me/entitlements':
            # Service entitlements bought on the PS Store: the project sells nothing, so the
            # list is empty. Field names from the retail libSceNpToolkit2.prx parser strings.
            if not online_id:
                return 403, {}, b'{"error":{"message":"project identity required"}}', 'webapi_entitlements', {}
            query = parse_qs(path.partition('?')[2])
            try:
                start, size = int(query['start'][0]), int(query['size'][0])
                int(query['service_label'][0])
                valid = query['entitlement_type'] == ['service'] and start >= 0 and 0 < size <= 1000
            except (KeyError, ValueError):
                valid = False
            if not valid:
                return 400, {}, b'{"error":{"message":"invalid entitlement request"}}', 'webapi_entitlements', {}
            payload = {'total_results': 0, 'entitlements': []}
            return 200, {'Content-Type': 'application/json'}, json.dumps(payload).encode(), 'webapi_entitlements', {'reply': payload}
        for (route_method, prefix), (status, payload) in WEBAPI_ROUTES.items():
            if route_method == method and path.startswith(prefix):
                return status, {'Content-Type': 'application/json'}, json.dumps(payload).encode(), 'webapi', {}
        return 404, {'Content-Type': 'application/json'}, \
            b'{"error":{"message":"not served by the CTR project server"}}', 'webapi', {}
    return 404, {'Content-Type': 'application/json'}, b'{"error":"not served"}', 'http', {}


async def handle_http(reader, writer):
    peer = '%s:%d' % writer.get_extra_info('peername')[:2]
    try:
        head = await reader.readuntil(b'\r\n\r\n')
        lines = head.decode('latin-1').split('\r\n')
        method, target = (lines[0].split(' ') + ['', ''])[:2]
        headers = {}
        for line in lines[1:]:
            if ':' in line:
                name, value = line.split(':', 1)
                headers[name.strip().lower()] = value.strip()
        body = await reader.readexactly(int(headers.get('content-length', '0') or 0))
    except (asyncio.IncompleteReadError, asyncio.LimitOverrunError, ConnectionError, ValueError):
        writer.close()
        return
    status, extra, data, event, fields = http_answer(method, target, headers, body)
    record(event=event, peer=peer, method=method, host=headers.get('host'), path=target,
           online_id=headers.get('x-ctr-online-id'), status=status,
           body=body.decode('utf-8', 'replace')[:4096], **fields)
    reason = {200: 'OK', 400: 'Bad Request', 404: 'Not Found'}.get(status, 'Status')
    head = 'HTTP/1.1 %d %s\r\n' % (status, reason)
    for name, value in extra.items():
        head += '%s: %s\r\n' % (name, value)
    head += 'Content-Length: %d\r\nConnection: close\r\n\r\n' % len(data)
    writer.write(head.encode() + data)
    await writer.drain()
    writer.close()


async def handle_tcp(reader, writer, port):
    peer = '%s:%d' % writer.get_extra_info('peername')[:2]
    record(event='tcp_accept', port=port, peer=peer)
    total = 0
    try:
        while True:
            data = await reader.read(65536)
            if not data:
                break
            total += len(data)
            record(event='tcp_data', port=port, peer=peer, length=len(data), hex=data.hex())
    except ConnectionError as error:
        record(event='tcp_error', port=port, peer=peer, error=str(error))
    record(event='tcp_close', port=port, peer=peer, total=total)
    writer.close()


async def handle_lobby(reader, writer):
    peer = '%s:%d' % writer.get_extra_info('peername')[:2]
    client = None
    try:
        header = await reader.readexactly(demonware_lobby.CLIENT_HEADER_SIZE)
        handshake = demonware_lobby.Handshake(header)
        writer.write(handshake.server_header())
        await writer.drain()
        size = struct.unpack('<I', await reader.readexactly(4))[0]
        auth = struct.pack('<I', size) + await reader.readexactly(size)
        found = handshake.complete(auth, list(sessions))
        if not found:
            record(event='lobby_auth_failed', peer=peer, frame=auth.hex())
            writer.write(demonware_lobby.frame(0x84, struct.pack('<I', 1)))
            await writer.drain()
            writer.close()
            return
        key, done, session = found
        user = sessions[key]
        if user['user_id'] in live_clients:
            record(event='lobby_identity_in_use',peer=peer,online_id=user['online_id'])
            writer.close()
            return
        record(event='lobby_connected', peer=peer, online_id=user['online_id'],
               connection_id=handshake.connection_id)
        writer.write(done)
        await writer.drain()
        client={'writer':writer,'crypto':session,'user':user,'send_lock':asyncio.Lock(),
                'ip':reachable_address(writer.get_extra_info('peername')[0])}
        live_clients[user['user_id']]=client
        while True:
            size = struct.unpack('<I', await reader.readexactly(4))[0]
            if size == 0:
                # Public protocol reference: h1-mod lobby_server.cpp54-59. A zero
                # frame is a keepalive and needs a zero frame back, not task dispatch.
                async with client['send_lock']:
                    writer.write(bytes(4))
                    await writer.drain()
                record(event='lobby_keepalive', peer=peer, online_id=user['online_id'])
                continue
            data = await reader.readexactly(size)
            if data[:2] != bytes([0xab, 0x85]):
                record(event='lobby_frame', peer=peer, hex=data.hex())
                continue
            payload_type, payload = session.open_record(data)
            if payload_type != 0x86:
                record(event='lobby_record', peer=peer, online_id=user['online_id'],
                       payload_type=payload_type, length=len(payload), hex=payload.hex())
                continue
            service, task, params = demonware_tasks.parse_request(payload)
            answer = demonware_tasks.handle(service, task, params, user)
            record(event='lobby_task', peer=peer, online_id=user['online_id'], service=service,
                   service_name=demonware_tasks.SERVICE_NAMES.get(service, '?'), task=task,
                   task_name=demonware_tasks.TASK_NAMES.get((service, task), '?'),
                   params=params, request_hex=payload.hex(), reply=answer.hex())
            await send_lobby(client,answer,demonware_tasks.LSG_TASK_REPLY)
            if (service,task)==(145,18):
                await create_ready_lobbies()
            elif (service,task)==(145,15):
                values=demonware_tasks.Reader(answer).values()
                if len(values)==6 and values[1][1]==0 and values[-1]==(demonware_tasks.BOOL,1):
                    await publish_host_document(params[0][1])
    except (asyncio.IncompleteReadError, ConnectionError) as error:
        record(event='lobby_closed', peer=peer, error=type(error).__name__)
    except ValueError as error:
        record(event='lobby_error', peer=peer, error=str(error))
    if client and live_clients.get(client['user']['user_id']) is client:
        del live_clients[client['user']['user_id']]
        with demonware_tasks.matchmaking_store.database:
            demonware_tasks.matchmaking_store.database.execute('UPDATE searches SET status=? '
                'WHERE account=? AND selector=? AND status=?',('cancelled',str(client['user']['user_id']),client['user']['room_token'],'pending'))
    writer.close()


class UdpRecorder(asyncio.DatagramProtocol):
    def __init__(self, port):
        self.port = port

    def datagram_received(self, data, address):
        record(event='udp_data', port=self.port, peer='%s:%d' % address[:2], length=len(data),
               hex=data.hex())


async def main():
    global server_key
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--host', default='0.0.0.0')
    parser.add_argument('--http', type=int, default=8780)
    parser.add_argument('--tcp', default='3075')
    parser.add_argument('--udp', default='3075,3478,3479')
    parser.add_argument('--peer-host', default='127.0.0.2',
                        help='address for UDP 3074 (Demonware STUN/QoS); empty to skip')
    parser.add_argument('--stun-secondary-host', default='127.0.0.3')
    parser.add_argument('--stun-alternate-port', type=int, default=3076)
    parser.add_argument('--public-host', default='',
                        help='public IPv4 for internet play: one-address discovery, loopback games '
                             'reported with this address, peer authorization for runtimes')
    parser.add_argument('--discovery-port', type=int, default=0,
                        help='first of four UDP discovery ports with --public-host (default 3078)')
    args = parser.parse_args()
    if args.public_host:
        socket.inet_aton(args.public_host)
        internet['public_host'] = args.public_host
        internet['discovery_port'] = args.discovery_port or 3078
    server_key = load_key()
    demonware_tasks.statistics_store = local_stats.Store(state / 'stats.sqlite3')
    demonware_tasks.matchmaking_store = local_matchmaking.Store(state / 'matchmaking.sqlite3')
    demonware_tasks.marketplace_store = local_marketplace.Store(state / 'marketplace.sqlite3')
    demonware_tasks.achievement_store = local_achievements.Store(state / 'achievements.sqlite3', demonware_rest.fields)
    global friends_store
    friends_store = local_friends.Store(state / 'friends.sqlite3')
    global session_store
    session_store = local_sessions.Store(state / 'sessions.sqlite3')
    loop = asyncio.get_running_loop()
    listening = []

    async def listen(name, factory):
        try:
            await factory()
            listening.append(name)
        except OSError as error:
            record(event='listen_failed', port=name, error=str(error))

    await listen('http/%d' % args.http, lambda: asyncio.start_server(handle_http, args.host, args.http))
    await listen('lobby/tcp/3074', lambda: asyncio.start_server(handle_lobby, args.host, 3074))
    for port in [int(p) for p in args.tcp.split(',') if p]:
        await listen('tcp/%d' % port, lambda port=port: asyncio.start_server(
            lambda r, w: handle_tcp(r, w, port), args.host, port))
    if args.public_host:
        base = internet['discovery_port']
        service = discovery.Service((args.public_host, base), (args.public_host, base + 2),
                                    base + 1, record, secondary_alternate_port=base + 3,
                                    public_host=args.public_host)
        for advertised in service.endpoints():
            bound = (args.host, advertised[1])
            await listen('discovery/%s:%d' % bound,
                         lambda advertised=advertised, bound=bound: loop.create_datagram_endpoint(
                             lambda: service.protocol(advertised), local_addr=bound))
    elif args.peer_host:
        service = discovery.Service((args.peer_host, 3074), (args.stun_secondary_host, 3074),
                                    args.stun_alternate_port, record)
        for host in (args.peer_host, args.stun_secondary_host):
            for port in (3074, args.stun_alternate_port):
                endpoint = (host, port)
                await listen('discovery/%s:%d' % endpoint,
                             lambda endpoint=endpoint: loop.create_datagram_endpoint(
                                 lambda: service.protocol(endpoint), local_addr=endpoint))
    for port in [int(p) for p in args.udp.split(',') if p]:
        await listen('udp/%d' % port, lambda port=port: loop.create_datagram_endpoint(
            lambda: UdpRecorder(port), local_addr=(args.host, port)))
    record(event='listening', host=args.host, ports=listening, log=str(log_path),
           traffic_key=str(state / 'server_key.der'), public_host=internet['public_host'],
           discovery_port=internet['discovery_port'])
    await asyncio.Event().wait()


if __name__ == '__main__':
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_file = log_path.open('w', encoding='utf-8')
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
