# SPDX-License-Identifier: GPL-2.0-or-later
"""Loopback integration against a freshly built C++ server. Synthetic accounts only."""
import argparse
import json
from pathlib import Path
import socket
import subprocess
import tempfile
import time
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen
from demonware_auth3 import user_id
from test_local_sessions import fixture


def run(server):
    root = Path(__file__).resolve().parents[2]
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0))
        port = probe.getsockname()[1]
    with tempfile.TemporaryDirectory(dir=root/'runtime/user') as temporary:
        with open(Path(temporary)/'server.log', 'wb') as log:
            process = subprocess.Popen([str(Path(server).resolve()), '--root', temporary,
                '--host', '127.0.0.1', '--http', str(port), '--tcp', '0', '--udp', '0',
                '--relay-port', '0'], stdout=log, stderr=log,
                creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
            def request(method, path, account=None, data=None, media='application/json'):
                headers = {'Content-Type':media}
                if account:
                    headers['X-Ctr-Online-Id'] = account
                req = Request(f'http://127.0.0.1:{port}{path}', data=data, headers=headers, method=method)
                try:
                    with urlopen(req, timeout=3) as response:
                        return response.status, response.read()
                except HTTPError as error:
                    return error.code, error.read()
            try:
                for _ in range(100):
                    if process.poll() is not None:
                        raise RuntimeError('server exited before HTTP readiness')
                    try:
                        if request('GET','/ctr/v1/config')[0] == 200:
                            break
                    except (URLError, TimeoutError):
                        time.sleep(.05)
                else:
                    raise RuntimeError('server startup timeout')
                presence='/webapi/sdk:userProfile/v1/users/'+str(user_id('InviteHost'))+'/presence'
                data=b'{"gameStatus":"Fixture lobby","gameData":"YWJj"}'
                assert request('PUT',presence+'/inGamePresence?notificationWithData=true','Intruder',data)[0] == 400
                assert request('PUT',presence+'/inGamePresence?notificationWithData=true','InviteHost',data)[0] == 204
                code,body=request('GET',presence+'/gameData','InviteGuest')
                assert code == 200 and json.loads(body)['gameData'] == 'YWJj'
                assert request('PUT',presence+'/gameData','InviteHost',b'{"gameData":"%%%"}')[0] == 400
                metadata = {'sessionPrivacy':'private','sessionMaxUser':8,'sessionName':'fixture','availablePlatforms':['PS4']}
                code, body = request('POST','/webapi/sdk:sessionInvitation/v1/sessions',
                                     'InviteHost',fixture(metadata),'multipart/mixed; boundary=test')
                assert code == 201, (code,body)
                session = json.loads(body)['sessionId']
                route = '/ctr/v1/session-invitations'
                payload = json.dumps({'sessionId':session,'recipients':[user_id('InviteGuest')]}).encode()
                assert request('POST',route,None,payload)[0] == 403
                assert request('POST',route,'Intruder',payload)[0] == 400
                code, body = request('POST',route,'InviteHost',payload)
                assert code == 201, (code,body)
                invite = json.loads(body)[0]['invitationId']
                assert json.loads(request('POST',route,'InviteHost',payload)[1])[0]['invitationId'] == invite
                assert json.loads(request('GET',route,'Intruder')[1]) == []
                for _ in range(2):
                    code, body = request('GET',route,'InviteGuest')
                    rows = json.loads(body)
                    assert code == 200 and len(rows) == 1 and rows[0]['sessionId'] == session
                    assert rows[0]['onlineId'] == 'InviteHost'
                assert request('DELETE',route+'/'+invite,'Intruder')[0] == 400
                session_route='/webapi/sdk:sessionInvitation/v1/sessions/'+session
                invitation_route='/webapi/sdk:sessionInvitation/v1/users/me/invitations/'+invite
                assert request('GET',session_route,'Intruder')[0] == 400
                assert request('PUT',route+'/'+invite,'Intruder',b'{}')[0] == 400
                code,body=request('PUT',route+'/'+invite,'InviteGuest',b'{}')
                assert code == 200 and json.loads(body)['sessionId'] == session
                assert json.loads(request('GET',route,'InviteGuest')[1]) == []
                code,body=request('GET',invitation_route+'?fields=@default,session,members,session.sessionLockFlag','InviteGuest')
                assert code == 200 and json.loads(body)['usedFlag'] is True
                assert request('GET',invitation_route+'/invitationData','InviteGuest') == (200,b'')
                assert request('GET',session_route+'/sessionData','InviteGuest') == (200,b'fixed')
                assert request('POST',session_route+'/members?index=0&priority=3','InviteGuest',b'')[0] == 204
                code,body=request('GET',session_route+'?fields=@default,members,sessionLockFlag','InviteGuest')
                assert code == 200 and json.loads(body)['memberCount'] == 2
                assert request('PUT',session_route+'/changeableSessionData','Intruder',b'wrong','application/octet-stream')[0] == 400
                assert request('PUT',session_route+'/changeableSessionData','InviteHost',b'new-data','application/octet-stream')[0] == 204
                assert request('GET',session_route+'/changeableSessionData','InviteGuest') == (200,b'new-data')
                users_route='/webapi/sdk:sessionInvitation/v1/users/@users/sessions?accountIds='+str(user_id('InviteGuest'))
                code,body=request('GET',users_route,'InviteGuest')
                assert code == 200 and json.loads(body)['users'][0]['sessions'][0]['priority'] == 3
                assert request('DELETE',session_route+'/members/me','InviteGuest')[0] == 204
                assert request('DELETE',route+'/'+invite,'InviteGuest')[0] == 204
                assert json.loads(request('GET',route,'InviteGuest')[1]) == []
                print('game_invitations_http: passed (synthetic loopback send/accept/session-data/membership/leave; no retail join)')
            finally:
                if process.poll() is None:
                    process.terminate()
                process.wait(timeout=10)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--server', required=True)
    run(parser.parse_args().server)
