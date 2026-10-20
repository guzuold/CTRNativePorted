# SPDX-License-Identifier: GPL-2.0-or-later
"""Two real local services and synthetic encrypted game clients; no game or Internet.

python -B tools/online/test_connection_authority.py path/to/ctr_online_server.exe
Reuses the existing protocol-210 test client. All state is temporary.
"""
import base64
import json
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import e2e_server_check as wire


def main():
    exe = str(Path(sys.argv[1]).resolve())
    processes, clients, logs = [], [], []
    addresses = ['127.0.0.11', '127.0.0.12']
    names = ['PolicyHost', 'PolicyGuest']
    wire.HTTP = 28780

    def http(index, method, path, body=b'', content_type='application/json'):
        wire.HOST = addresses[index]
        return wire.http(method, path, 'ctr-online',
                         {'X-CTR-Online-ID': names[index], 'Content-Type': content_type}, body)

    def allowed(index, address):
        return http(index, 'GET', '/ctr/v1/peer-allowed?ip=' + address)[0]

    def check(value, label):
        assert value, label
        print('PASS ' + label, flush=True)

    with tempfile.TemporaryDirectory(prefix='ctr_policy_e2e_') as folder:
        roots = [Path(folder) / 'host', Path(folder) / 'guest']
        flags = getattr(subprocess, 'CREATE_NO_WINDOW', 0)
        try:
            codes = [subprocess.check_output([exe, '--root', str(root), '--friend-code'],
                     creationflags=flags, text=True).strip() for root in roots]
            for i in range(2):
                subprocess.run([exe, '--root', str(roots[i]), '--add-friend', codes[1-i],
                                addresses[1-i] + ':28780'], check=True, capture_output=True, creationflags=flags)
                log = open(Path(folder) / ('server%d.log' % i), 'wb')
                logs.append(log)
                processes.append(subprocess.Popen([exe, '--root', str(roots[i]), '--host', addresses[i],
                    '--public-host', addresses[i], '--http', '28780', '--discovery-port', '28781',
                    '--relay-port', '0', '--tcp', '', '--udp', '', '--friend-network', '--local-name', names[i]],
                    stdout=log, stderr=log, creationflags=flags))
            accounts = []
            for i in range(2):
                for attempt in range(50):
                    try:
                        if http(i, 'GET', '/ctr/v1/config')[0] == 200:
                            break
                    except OSError:
                        pass
                    time.sleep(.1)
                wire.HOST = addresses[i]
                reply = wire.http('POST', '/auth/', 'ctr-ps4-auth3.prod.demonware.net',
                    {'X-CTR-Online-ID': names[i]}, json.dumps({'auth_task': '46', 'iv_seed': '1588948653',
                     'title_id': '5760', 'identity': 'x', 'extra_data': '{}'}).encode())[3]
                ticket = base64.b64decode(json.loads(reply)['server_ticket'])
                accounts.append(int.from_bytes(ticket[24:32], 'little'))
                client = wire.Lobby(ticket[:24])
                clients.append(client)
                check(client.result == 'connected', 'encrypted game client connected to its own service')
            time.sleep(5.5)  # signed presence round reports both games online
            check(allowed(0, addresses[1]) == 403 and allowed(1, addresses[0]) == 403,
                  'friendship alone does not authorize gameplay')
            meta = json.dumps({'sessionPrivacy': 'private', 'sessionMaxUser': 8,
                               'sessionName': 'test', 'availablePlatforms': ['PS4']})
            multipart = ('--B\r\nContent-Type: application/json\r\nContent-Description: session-request\r\n'
                         'Content-Length: %d\r\n\r\n%s\r\n--B--\r\n' % (len(meta), meta)).encode()
            status, _, _, reply = http(0, 'POST', '/webapi/sdk:sessionInvitation/v1/sessions',
                                       multipart, 'multipart/mixed; boundary=B')
            check(status == 201, 'host creates session')
            session = json.loads(reply)['sessionId']
            status, _, _, reply = http(0, 'POST', '/ctr/v1/session-invitations',
                json.dumps({'sessionId': session, 'recipients': [accounts[1]]}).encode())
            check(status == 201, 'invitation delivered across services')
            invitation = json.loads(reply)[0]['invitationId']
            check(http(1, 'PUT', '/ctr/v1/session-invitations/' + invitation)[0] == 200, 'guest accepts')
            check(allowed(1, addresses[0]) == 200 and allowed(0, addresses[1]) == 200,
                  'local authorization route permits both directions BEFORE membership')
            check(allowed(1, '203.0.113.99') == 403, 'unknown destination remains blocked')
            route = '/webapi/sdk:sessionInvitation/v1/sessions/' + session + '/members/me'
            check(http(1, 'DELETE', route)[0] == 204, 'failed join can leave without invalid-member error')
            check(allowed(1, addresses[0]) == 403 and allowed(0, addresses[1]) == 403,
                  'both local routes revoke the cancelled join')
            # JOIN from the friends list: the game reads the host's Delta record (138/14) and
            # contacts the host at once, before any session request names it.
            clients[0].send_task(138, 1, wire.typed((wire.STRING, 'Delta'), (19, bytes(67)), (8, 1), (8, 2), (19, b'c')))
            check(clients[0].receive()[0] == 1, 'host publishes its Delta session')
            time.sleep(5.5)  # the guest's service receives it with the host's presence
            check(allowed(1, addresses[0]) == 403, 'a published Delta session alone does not authorize')
            clients[1].send_task(138, 14, wire.typed((wire.STRING, 'Delta'), (wire.UINT64, accounts[0])))
            kind, payload = clients[1].receive()
            check(kind == 1 and len(payload) > 40, "guest's game receives the host's Delta record")
            check(allowed(1, addresses[0]) == 200, 'guest may contact the host after reading its Delta record')
            check(allowed(0, addresses[1]) == 200, 'host accepts the joining friend announced by its service')
            check(allowed(1, '203.0.113.99') == 403, 'unknown destination still blocked during a Delta join')
        finally:
            for client in clients:
                client.close()
            for process in processes:
                process.terminate()
                process.wait(timeout=10)
            for log in logs:
                log.close()
    return 0


if __name__ == '__main__':
    sys.exit(main())
