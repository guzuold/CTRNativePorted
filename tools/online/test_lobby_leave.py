# SPDX-License-Identifier: GPL-2.0-or-later
"""The host of a public lobby leaves it (145/10) and cancels its search (145/8): the remaining
player must become the host, be told so, and be able to update the lobby as host.

Synthetic players on one service at 127.0.0.2 (default ports: close the launcher first).
Usage: python -B tools/online/test_lobby_leave.py [--server build/CTRPort-claude/ctr_online_server.exe]
"""
import argparse, json, sys, tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_public_hub as h
from demonware_tasks import Reader, STRING, UINT64

ROOM_UPDATED_EVENT = 2006


def task_reply(player):
    """The next task reply on the player's lobby connection; pushed records are skipped."""
    while True:
        kind, payload = player['lobby'].receive()
        if kind == 1:
            return kind, payload


def reply_error(payload):
    values = Reader(payload).values()
    return values[1][1] if len(values) > 1 else None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--server', default=str(h.ROOT / 'build/CTRPort-claude/ctr_online_server.exe'))
    args = parser.parse_args()
    process = h.start(str(Path(args.server).resolve()), h.HUB, Path(tempfile.mkdtemp(prefix='ctr_leave_')) / 'hub')
    try:
        alpha, bravo = h.login(h.HUB, 'HostAlpha'), h.login(h.HUB, 'GuestBravo')
        h.advertise(alpha, '198.51.100.10'); h.advertise(bravo, '198.51.100.11')
        kind, payload = h.search(alpha)
        alpha_search = json.loads([v for t, v in Reader(payload).values() if t == STRING][0])['mm_id']
        h.search(bravo)
        host, created = h.first_push([alpha, bravo])
        h.check(host is alpha, 'first searcher hosts')
        lobby_id, host_doc = created[2][1], json.loads(created[4][1])
        host_doc['player_state'][str(bravo['user'])] = 1
        host_doc['attachment'] = ''
        alpha['lobby'].send_task(145, 15, h.e2e.typed((UINT64, lobby_id), (UINT64, 1), (STRING, json.dumps(host_doc))))
        alpha['lobby'].receive()
        joined = {}
        for _ in range(2):
            player, values = h.first_push([alpha, bravo])
            if player is not None:
                joined[player['name']] = values
        h.check('GuestBravo' in joined, 'guest joins the lobby')

        # The host leaves the lobby and cancels its search, as the retail game does.
        alpha['lobby'].send_task(145, 10, h.e2e.typed((UINT64, lobby_id)))
        kind, payload = task_reply(alpha)
        h.check(reply_error(payload) == 0, 'leave lobby (145/10) answered without error')
        alpha['lobby'].send_task(145, 8, h.e2e.typed((UINT64, alpha_search)))
        kind, payload = task_reply(alpha)
        h.check(reply_error(payload) == 0, 'cancel search (145/8) answered without error')

        player, update = h.first_push([bravo], timeout=5)
        promoted = None
        if player is bravo and update and update[0][1] == ROOM_UPDATED_EVENT:
            promoted = json.loads(update[3][1])
        h.check(promoted is not None and promoted['listen_server']['host_player_id'] == bravo['user'],
                'remaining player is told it is now the host')
        if promoted:
            promoted['player_state'] = {str(bravo['user']): 2}
            promoted['attachment'] = ''
            promoted.pop('ctr_migration', None)
            bravo['lobby'].send_task(145, 15, h.e2e.typed((UINT64, lobby_id), (UINT64, promoted['update_id']), (STRING, json.dumps(promoted))))
            kind, payload = task_reply(bravo)
            values = Reader(payload).values()
            h.check(reply_error(payload) == 0 and values[-1][1] in (True, 1), 'new host can update the lobby')

        # The remaining player leaves too; both search again and must meet in a new lobby.
        bravo['lobby'].send_task(145, 10, h.e2e.typed((UINT64, lobby_id)))
        kind, payload = task_reply(bravo)
        h.check(reply_error(payload) == 0, 'last player leaves the lobby')
        for player, address in ((alpha, '198.51.100.10'), (bravo, '198.51.100.11')):
            info = {'listen_server': {'local_address': h.base64.b64encode(bytes(60) + bytes(int(x) for x in address.split('.')) + bytes(3)).decode(),
                                      'security_id': h.base64.b64encode(bytes(8)).decode(),
                                      'security_key': h.base64.b64encode(bytes(16)).decode(), 'nat': 2},
                    'dedicated_server': {'build_name': 'hub'}}
            player['lobby'].send_task(145, 2, h.e2e.typed((STRING, json.dumps(info)))); task_reply(player)
            player['lobby'].send_task(145, 3); kind, payload = task_reply(player)
            player['token'] = Reader(payload).values()[-1][1]
        for player in (alpha, bravo):
            document = {'members': [player['user']], 'tokens': [player['token']], 'lobby_slots': {'min': 2, 'max': 8},
                        'dedicated_server': {'request_level': 'not_needed'}, 'team_balance': {'num_teams': 0},
                        'ruleset_payload': {'filter': {'matchmaking_version': 200, 'playlist_id': 0}},
                        'player_id_to_slots': {str(player['user']): 0}, 'performance': {'game_type': 1}}
            player['lobby'].send_task(145, 18, h.e2e.typed((STRING, json.dumps(document)))); task_reply(player)
        created = None
        for _ in range(6):
            player, values = h.first_push([alpha, bravo])
            if player is None: break
            if values and values[0][1] == 2005: created = values; break
        h.check(created is not None and created[2][1] != lobby_id, 'both search again and meet in a new lobby')
        alpha['lobby'].close(); bravo['lobby'].close()
    finally:
        process.kill()
    print(json.dumps({'scope': 'lobby_leave', 'failures': h.failures}))
    return 1 if h.failures else 0


if __name__ == '__main__':
    sys.exit(main())
