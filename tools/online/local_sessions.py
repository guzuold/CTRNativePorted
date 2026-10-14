# SPDX-License-Identifier: GPL-2.0-or-later
"""Persistent project sessions using the multipart format observed on retail.

Session metadata and attachments stay in the ignored runtime user database.
No console service is contacted and no session is created on an external account.
"""
from email.message import Message
import json
import sqlite3
import uuid


def parse_parts(content_type, body):
    message = Message()
    message['Content-Type'] = content_type
    boundary = message.get_param('boundary')
    if message.get_content_type() != 'multipart/mixed' or not boundary or len(body) > 2*1024*1024:
        raise ValueError('unsupported session multipart')
    marker = b'--' + boundary.encode('ascii')
    offset = 0
    parts = []
    while body[offset:offset+len(marker)] == marker:
        offset += len(marker)
        if body[offset:offset+2] == b'--':
            if body[offset+2:].strip(b'\r\n'):
                raise ValueError('trailing multipart data')
            return parts
        if body[offset:offset+2] != b'\r\n':
            raise ValueError('invalid multipart delimiter')
        offset += 2
        end = body.index(b'\r\n\r\n', offset)
        headers = {}
        for line in body[offset:end].decode('ascii').split('\r\n'):
            name, value = line.split(':',1)
            headers[name.lower()] = value.strip()
        size = int(headers['content-length'])
        if size < 0 or len(parts) >= 8:
            raise ValueError('invalid session part length')
        offset = end + 4
        data = body[offset:offset+size]
        if len(data) != size:
            raise ValueError('truncated session part')
        name = headers['content-description']
        parts.append((name,headers['content-type'],data))
        offset += size
        # The original retail Toolkit sends this final part without a closing
        # MIME delimiter. Accept only exact EOF after its declared payload.
        if offset == len(body) and name == 'changeable-session-data':
            return parts
        if body[offset:offset+2] != b'\r\n':
            raise ValueError('missing session part separator')
        while body[offset:offset+2] == b'\r\n':
            offset += 2
    raise ValueError('missing multipart terminator')


class Store:
    def __init__(self, path):
        self.database = sqlite3.connect(path)
        self.database.execute('CREATE TABLE IF NOT EXISTS sessions ('
                              'id TEXT PRIMARY KEY, owner TEXT NOT NULL, metadata TEXT NOT NULL)')
        self.database.execute('CREATE TABLE IF NOT EXISTS session_parts ('
                              'session TEXT NOT NULL, name TEXT NOT NULL, media TEXT NOT NULL, '
                              'data BLOB NOT NULL, PRIMARY KEY(session,name))')
        self.database.commit()

    def create(self, account, content_type, body):
        parts = parse_parts(content_type, body)
        names = [p[0] for p in parts]
        allowed = {'session-request','session-image','session-data','changeable-session-data'}
        if not parts or names[0] != 'session-request' or len(set(names)) != len(names) or not set(names) <= allowed:
            raise ValueError('unsupported session parts')
        metadata = json.loads(parts[0][2])
        if not isinstance(metadata, dict) or metadata.get('sessionPrivacy') not in ('private','public') or type(metadata.get('sessionMaxUser')) is not int or not 1 <= metadata['sessionMaxUser'] <= 8 or not isinstance(metadata.get('sessionName'), str) or metadata.get('availablePlatforms') != ['PS4']:
            raise ValueError('unsupported session metadata')
        session = uuid.uuid4().hex
        with self.database:
            self.database.execute('INSERT INTO sessions VALUES (?,?,?)',
                                  (session,str(account),json.dumps(metadata)))
            self.database.executemany('INSERT INTO session_parts VALUES (?,?,?,?)',
                                      [(session,name,media,data) for name,media,data in parts[1:]])
        return {'sessionId':session}

    def close(self):
        self.database.close()
