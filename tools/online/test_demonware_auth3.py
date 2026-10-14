# SPDX-License-Identifier: GPL-2.0-or-later
"""Checks of the project server's auth3 reply (tools/online/demonware_auth3.py).

Run from the project root: python -B tools/online/test_demonware_auth3.py
- the request is the one the retail v01.21 client sent in run play_20261014_195257;
- the X-Signature is verified independently with OpenSSL (RSA-PSS, SHA-256, salt 0) over the
  exact body bytes;
- the reply is checked against the client's parser (auth3 reply handler at 0x13d30 and
  ticket deserializer at 0x144d0 in v01.21): task 47, code 700, 0x80-byte tickets, magic
  0xEFBDADDE first, fields at their offsets, no "identity" key, extended_data present.
"""
import base64, json, shutil, struct, subprocess, sys, tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import demonware_auth3, rsa_pss

REQUEST = {"auth_task": "46", "iv_seed": "1588948653", "title_id": "5760", "identity": "356c4bc3",
           "extra_data": json.dumps({"version": "0", "token": json.dumps(
               {"scope": "openid,openid:duid", "id_token": "x.y.ctrlocal"}), "extended_data": True})}
failures = 0


def check(condition, what):
    global failures
    print(json.dumps({'scope': 'auth3_server_test', 'check': what, 'ok': bool(condition)}))
    failures += not condition


key = rsa_pss.generate()
status, headers, body, session = demonware_auth3.reply(REQUEST, 'CTRTester1', key)
check(status == 200 and 'X-Signature' in headers, 'HTTP 200 with X-Signature')
reply = json.loads(body)
check(reply['auth_task'] == '47' and reply['code'] == '700' and reply['iv_seed'] == '1588948653',
      'task 47, code 700, iv_seed echoed')
check('identity' not in reply and json.loads(reply['extra_data'])['extended_data'] == '{}',
      'no identity key, extended_data present')

ticket = base64.b64decode(reply['client_ticket'])
magic, kind, title, issued, expires, license_id, user = struct.unpack_from('<IBIIIQQ', ticket)
name = ticket[0x21:0x61].rstrip(b'\0').decode()
session_key = ticket[0x61:0x79]
check(len(ticket) == 0x80 and magic == 0xEFBDADDE and kind == 0 and title == 5760,
      'client ticket size, magic, type, title')
check(expires > issued and license_id == 0 and user == demonware_auth3.user_id('CTRTester1')
      and name == 'CTRTester1', 'client ticket times, user ID, user name')
check(session_key.hex() == session['session_key'] and ticket[0x79:] == b'\0' * 7,
      'client ticket session key and zero hash fields')
server = base64.b64decode(reply['server_ticket'])
check(len(server) == 0x80 and server[:24] == session_key, 'server ticket carries the session key')
check(demonware_auth3.reply(dict(REQUEST, auth_task='12'), 'CTRTester1', key)[0] == 400,
      'other auth tasks refused')

openssl = shutil.which('openssl')
if openssl:
    with tempfile.TemporaryDirectory() as directory:
        d = Path(directory)
        (d / 'key.der').write_bytes(rsa_pss.spki_der(key['n'], key['e']))
        (d / 'body').write_bytes(body)
        (d / 'sig').write_bytes(base64.b64decode(headers['X-Signature']))
        subprocess.run([openssl, 'pkey', '-pubin', '-inform', 'DER', '-in', d / 'key.der',
                        '-out', d / 'key.pem'], check=True)
        result = subprocess.run([openssl, 'dgst', '-sha256', '-verify', d / 'key.pem', '-sigopt',
                                 'rsa_padding_mode:pss', '-sigopt', 'rsa_pss_saltlen:0',
                                 '-signature', d / 'sig', d / 'body'], capture_output=True, text=True)
        check(result.returncode == 0 and 'Verified OK' in result.stdout,
              'X-Signature verifies with OpenSSL (PSS, SHA-256, salt 0)')
        (d / 'body').write_bytes(body.replace(b'"700"', b'"701"'))
        result = subprocess.run([openssl, 'dgst', '-sha256', '-verify', d / 'key.pem', '-sigopt',
                                 'rsa_padding_mode:pss', '-sigopt', 'rsa_pss_saltlen:0',
                                 '-signature', d / 'sig', d / 'body'], capture_output=True, text=True)
        check(result.returncode != 0, 'altered body fails verification')
    check(len(rsa_pss.spki_der(key['n'], key['e'])) == 0x126, 'public key DER is 0x126 bytes')
else:
    check(False, 'openssl available for the independent signature check')

print(json.dumps({'scope': 'auth3_server_test', 'passed': failures == 0, 'failures': failures}))
sys.exit(1 if failures else 0)
