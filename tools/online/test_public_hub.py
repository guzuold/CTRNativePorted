# SPDX-License-Identifier: GPL-2.0-or-later
"""Public matchmaking across two services (phase 2), at the lobby protocol level.

Two C++ project servers: the hub on 127.0.0.2 and a member service on 127.0.0.3 started
with --match-hub. HubAlpha's game talks to the hub, MemberBravo's game to its own service,
which forwards the public matchmaking tasks to the hub and delivers the hub's pushed events.
The two players must end up in the same lobby. Synthetic players with the e2e harness's
lobby client; no retail game is involved.

    python tools/online/test_public_hub.py [--server build/CTRPort-dev/ctr_online_server.exe]
"""
import argparse, base64, json, os, select, shutil, struct, subprocess, sys, tempfile, time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import e2e_server_check as e2e
from demonware_tasks import Reader, STRING, UINT64

ROOT = Path(__file__).resolve().parents[2]
HUB, MEMBER = '127.0.0.2', '127.0.0.3'
failures = 0


def check(ok, what):
    global failures
    print(json.dumps({'scope': 'public_hub', 'check': what, 'ok': bool(ok)}), flush=True)
    if not ok:
        failures += 1


def at(host):
    e2e.HOST, e2e.HTTP = host, 8780


def wait_http(host, deadline=20):
    end = time.time() + deadline
    while time.time() < end:
        try:
            at(host)
            if e2e.http('GET', '/ctr/v1/config')[0] == 200:
                return True
        except OSError:
            time.sleep(0.2)
    return False


def start(exe, host, root, extra=()):
    args = [exe, '--host', host, '--public-host', host, '--tcp', '', '--udp', '', '--relay-port', '0',
            '--root', str(root)] + list(extra)
    process = subprocess.Popen(args, cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not wait_http(host):
        process.kill()
        raise RuntimeError('server on %s did not start' % host)
    return process


def login(host, name):
    at(host)
    request = {'auth_task': '46', 'iv_seed': '1588948653', 'title_id': '5760', 'identity': 'x', 'extra_data': '{}'}
    status, line, fields, body = e2e.http('POST', '/auth/', 'ctr-ps4-auth3.prod.demonware.net',
                                          {'X-CTR-Online-ID': name, 'Content-Type': 'application/json'},
                                          json.dumps(request).encode())
    reply = json.loads(body)
    server = base64.b64decode(reply['server_ticket'])
    user = struct.unpack_from('<Q', server, 24)[0]
    lobby = e2e.Lobby(server[:24])
    return {'name': name, 'user': user, 'lobby': lobby}


def advertise(player, address):
    block = bytearray(67)
    block[60:64] = bytes(int(x) for x in address.split('.'))
    info = {'listen_server': {'local_address': base64.b64encode(bytes(block)).decode(),
                              'security_id': base64.b64encode(bytes(8)).decode(),
                              'security_key': base64.b64encode(bytes(16)).decode(), 'nat': 2},
            'dedicated_server': {'build_name': 'hub'}}
    lobby = player['lobby']
    lobby.send_task(145, 2, e2e.typed((STRING, json.dumps(info))))
    kind, payload = lobby.receive()
    lobby.send_task(145, 3)
    kind, payload = lobby.receive()
    player['token'] = Reader(payload).values()[-1][1]
    return player['token']


def search(player):
    account = player['user']
    document = {'members': [account], 'tokens': [player['token']], 'lobby_slots': {'min': 2, 'max': 8},
                'dedicated_server': {'request_level': 'not_needed'}, 'team_balance': {'num_teams': 0},
                'ruleset_payload': {'filter': {'matchmaking_version': 200, 'playlist_id': 0}},
                'player_id_to_slots': {str(account): 0}, 'performance': {'game_type': 1}}
    player['lobby'].send_task(145, 18, e2e.typed((STRING, json.dumps(document))))
    return player['lobby'].receive()


def first_push(players, timeout=10):
    """The first pushed record to any of the players: (player, values)."""
    sockets = {p['lobby'].s: p for p in players}
    ready, _, _ = select.select(list(sockets), [], [], timeout)
    if not ready:
        return None, None
    player = sockets[ready[0]]
    kind, payload = player['lobby'].receive()
    return player, Reader(payload).values()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--server', default=str(ROOT / 'build/CTRPort-dev/ctr_online_server.exe'))
    args = parser.parse_args()
    exe = str(Path(args.server).resolve())
    work = Path(tempfile.mkdtemp(prefix='ctr_hub_'))
    processes = []
    try:
        processes.append(start(exe, HUB, work / 'hub'))
        at(HUB)
        nonce = os.urandom(16).hex()
        status, line, fields, body = e2e.http('GET', '/ctr/v1/identity?nonce=' + nonce)
        hub_code = json.loads(body)['friend_code']
        processes.append(start(exe, MEMBER, work / 'member', ['--match-hub', hub_code + '@' + HUB]))

        # Round 1: the hub's own player searches first and hosts; round 2: the member's player
        # searches first, so the lobby creation itself travels through the member's service.
        for hub_name, member_name, member_first in [('HubAlpha', 'MemberBravo', False), ('HubCharlie', 'MemberDelta', True)]:
            alpha = login(HUB, hub_name)
            bravo = login(MEMBER, member_name)
            check(alpha['lobby'].result == 'connected' and bravo['lobby'].result == 'connected',
                  'each game connects to its own service')
            token_alpha, token_bravo = advertise(alpha, '198.51.100.10'), advertise(bravo, '198.51.100.11')
            check(token_alpha and token_bravo and token_alpha != token_bravo,
                  'member player gets its matchmaking token from the hub')
            for player in ([bravo, alpha] if member_first else [alpha, bravo]):
                kind, payload = search(player)
                values = Reader(payload).values()
                check(kind == 1 and any(t == STRING and 'mm_id' in v for t, v in values),
                      '%s search answered by the hub' % player['name'])

            host, created = first_push([alpha, bravo])
            check(host is not None and len(created) >= 5, 'lobby created for the two players (host notified)')
            if host is None:
                return
            check(host is (bravo if member_first else alpha), 'first searcher hosts (%s)' % host['name'])
            guest = bravo if host is alpha else alpha
            lobby_id, host_doc = created[2][1], json.loads(created[4][1])
            host_doc['player_state'][str(guest['user'])] = 1
            host_doc['attachment'] = ''
            host['lobby'].send_task(145, 15, e2e.typed((UINT64, lobby_id), (UINT64, 1), (STRING, json.dumps(host_doc))))
            kind, payload = host['lobby'].receive()
            check(kind == 1 and Reader(payload).values()[-1][1] in (True, 1), 'host document synced at the hub')
            pushed = {}
            for _ in range(2):
                player, values = first_push([alpha, bravo])
                if player is not None:
                    pushed[player['name']] = values
            joined = pushed.get(guest['name'])
            check(joined is not None and any(t == UINT64 and v == lobby_id for t, v in joined),
                  'guest %s receives the join for the same lobby' % guest['name'])
            check(host['name'] in pushed, 'host %s receives the document update' % host['name'])

            bravo['lobby'].close()
            time.sleep(1.5)
            log = sorted((work / 'hub' / 'runtime/user/logs').glob('online_server_*.jsonl'))
            rows = [json.loads(line) for line in log[-1].read_text(encoding='utf-8').splitlines()] if log else []
            mine = [row.get('event') for row in rows if row.get('online_id') == member_name]
            check('hub_player_joined' in mine and 'hub_task' in mine, 'hub records %s and its tasks' % member_name)
            check('hub_player_left' in mine, '%s disconnect signs the player off at the hub' % member_name)
            alpha['lobby'].close()
            time.sleep(0.5)
    finally:
        for process in processes:
            process.kill()
            process.wait()
        shutil.rmtree(work, ignore_errors=True)
    print(json.dumps({'scope': 'public_hub', 'failures': failures}))


if __name__ == '__main__':
    main()
    sys.exit(1 if failures else 0)
