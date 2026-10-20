# SPDX-License-Identifier: GPL-2.0-or-later
"""Persistent project sessions using the multipart format observed on retail.

Session metadata and attachments stay in the ignored runtime user database.
No console service is contacted and no session is created on an external account.
"""
from email.message import Message
import json
import sqlite3
import uuid
import time
import demonware_auth3


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
        self.database.execute('CREATE TABLE IF NOT EXISTS session_members ('
                              'session TEXT, account TEXT, name TEXT, PRIMARY KEY(session,account))')
        self.database.execute('CREATE TABLE IF NOT EXISTS session_admitted ('
                              'session TEXT, account TEXT, PRIMARY KEY(session,account))')
        self.database.execute('CREATE TABLE IF NOT EXISTS game_invitations ('
                              'id TEXT PRIMARY KEY, session TEXT, sender TEXT, account TEXT, recipient TEXT, '
                              'expires INTEGER, accepted INTEGER DEFAULT 0)')
        self.database.execute('CREATE TABLE IF NOT EXISTS session_priorities (session TEXT, account TEXT, value INTEGER, PRIMARY KEY(session,account))')
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
        metadata.setdefault("sessionLockFlag", False)
        metadata["sessionCreateTimestamp"] = int(time.time()) * 1000
        session = uuid.uuid4().hex
        with self.database:
            self.database.execute('INSERT INTO sessions VALUES (?,?,?)',
                                  (session,str(account),json.dumps(metadata)))
            self.database.executemany('INSERT INTO session_parts VALUES (?,?,?,?)',
                                      [(session,name,media,data) for name,media,data in parts[1:]])
        with self.database:
            self.database.execute('INSERT INTO session_members VALUES (?,?,?)',(session,str(account),''))
        return {'sessionId':session}

    def update(self, account, session, body):
        row = self.database.execute('SELECT metadata FROM sessions WHERE id=? AND owner=?',
                                    (session,str(account))).fetchone()
        if row is None:
            raise ValueError('unknown session or not its owner')
        change = json.loads(body)
        if not isinstance(change,dict) or not change:
            raise ValueError('unsupported session update')
        for name,value in change.items():
            valid = ((name == 'sessionPrivacy' and value in ('public','private')) or
                     (name == 'sessionMaxUser' and type(value) is int and 1 <= value <= 8) or
                     (name == 'sessionLockFlag' and type(value) is bool) or
                     (name == 'sessionStatus' and isinstance(value,str)))
            if not valid:
                raise ValueError('unsupported session update field')
        metadata = json.loads(row[0])
        metadata.update(change)
        with self.database:
            self.database.execute('UPDATE sessions SET metadata=? WHERE id=?',(json.dumps(metadata),session))

    def _expire(self, now):
        with self.database:
            self.database.execute('DELETE FROM game_invitations WHERE expires<=?',(now,))

    def invite(self, account, sender, session, recipients, now):
        self._expire(now)
        owner = self.database.execute('SELECT owner FROM sessions WHERE id=?',(session,)).fetchone()
        if not owner or owner[0] != str(account) or demonware_auth3.user_id(sender) != account:
            raise ValueError('invitation sender must own session')
        if (not 1 <= len(recipients) <= 16 or len(set(recipients)) != len(recipients) or
            any(type(r) is not int or not 0 < r < 2**64 or r == account for r in recipients)):
            raise ValueError('invalid invitation recipients')
        result = []
        with self.database:
            for recipient in recipients:
                existing = self.database.execute('SELECT id FROM game_invitations WHERE session=? AND account=? AND recipient=? AND accepted=0',
                                                  (session,str(account),str(recipient))).fetchone()
                if existing:
                    invitation = existing[0]
                else:
                    count = self.database.execute('SELECT COUNT(*) FROM game_invitations WHERE recipient=?',(str(recipient),)).fetchone()[0]
                    if count >= 64:
                        raise ValueError('recipient invitation queue full')
                    invitation = uuid.uuid4().hex
                    self.database.execute('INSERT INTO game_invitations VALUES (?,?,?,?,?,?,0)',
                                          (invitation,session,sender,str(account),str(recipient),now+300))
                result.append({'invitationId':invitation,'accountId':str(recipient)})
        return result

    def invitations(self, recipient, now):
        self._expire(now)
        return [dict(zip(('invitationId','sessionId','onlineId','accountId'), row)) for row in
                self.database.execute('SELECT id,session,sender,account FROM game_invitations WHERE recipient=? AND accepted=0 ORDER BY id',(str(recipient),))]

    def _invitation(self, recipient, invitation, now):
        self._expire(now)
        row = self.database.execute('SELECT session,sender,account,accepted FROM game_invitations WHERE id=? AND recipient=?',
                                    (invitation,str(recipient))).fetchone()
        if row is None:
            raise ValueError('unknown invitation')
        return row

    def dismiss(self, recipient, invitation, now):
        self._invitation(recipient,invitation,now)
        with self.database:
            self.database.execute('DELETE FROM game_invitations WHERE id=?',(invitation,))

    def accept(self, recipient, invitation, now):
        session,_,_,_ = self._invitation(recipient,invitation,now)
        metadata = self._readable(recipient,session)
        if metadata.get('sessionLockFlag'):
            raise ValueError('session locked')
        with self.database:
            self.database.execute('INSERT OR IGNORE INTO session_admitted VALUES (?,?)',(session,str(recipient)))
            self.database.execute('UPDATE game_invitations SET accepted=1 WHERE id=?',(invitation,))
        return {'sessionId':session,'invitationId':invitation}

    def _readable(self, account, session):
        row = self.database.execute('SELECT owner,metadata FROM sessions WHERE id=?',(session,)).fetchone()
        if row is None:
            raise ValueError('unknown session')
        owner, text = row
        metadata = json.loads(text)
        admitted = self.database.execute('SELECT 1 FROM session_admitted WHERE session=? AND account=?',(session,str(account))).fetchone()
        member = self.database.execute('SELECT 1 FROM session_members WHERE session=? AND account=?',(session,str(account))).fetchone()
        invited = self.database.execute('SELECT 1 FROM game_invitations WHERE session=? AND recipient=? AND expires>?',(session,str(account),int(time.time()))).fetchone()
        if owner != str(account) and not (admitted or member or invited) and metadata['sessionPrivacy'] != 'public':
            raise ValueError('private session')
        return metadata

    def describe(self, account, session):
        result = self._readable(account,session)
        owner = self.database.execute('SELECT owner FROM sessions WHERE id=?',(session,)).fetchone()[0]
        members = [{'accountId':a,'onlineId':n,'platform':'PS4'} for a,n in self.database.execute(
                   'SELECT account,name FROM session_members WHERE session=? ORDER BY account',(session,))]
        result.update(sessionId=session,members=members,memberCount=len(members))
        result['sessionCreator'] = next(m for m in members if m['accountId'] == owner)
        return result

    def invitation(self, recipient, invitation, now):
        session,sender,account,accepted = self._invitation(recipient,invitation,now)
        return {'invitationId':invitation,'sessionId':session,'usedFlag':bool(accepted),'expired':False,
                'fromUser':{'onlineId':sender,'accountId':account},'session':self.describe(recipient,session)}

    def join(self, account, name, session):
        metadata = self._readable(account,session)
        exists = self.database.execute('SELECT 1 FROM session_members WHERE session=? AND account=?',(session,str(account))).fetchone()
        count = self.database.execute('SELECT COUNT(*) FROM session_members WHERE session=?',(session,)).fetchone()[0]
        if not exists and (metadata.get('sessionLockFlag') or count >= metadata['sessionMaxUser']):
            raise ValueError('session locked or full')
        with self.database:
            self.database.execute('INSERT OR REPLACE INTO session_members VALUES (?,?,?)',(session,str(account),name))

    def erase(self, account, session):
        owner = self.database.execute('SELECT owner FROM sessions WHERE id=?',(session,)).fetchone()
        if owner != (str(account),):
            raise ValueError('not session owner')
        with self.database:
            for table in ('game_invitations','session_members','session_admitted','session_parts','session_priorities'):
                self.database.execute('DELETE FROM '+table+' WHERE session=?',(session,))
            self.database.execute('DELETE FROM sessions WHERE id=?',(session,))

    def leave(self, account, session):
        owner = self.database.execute('SELECT owner FROM sessions WHERE id=?',(session,)).fetchone()
        if owner == (str(account),):
            self.erase(account,session)
            return
        with self.database:
            if not self.database.execute('DELETE FROM session_members WHERE session=? AND account=?',(session,str(account))).rowcount:
                raise ValueError('not session member')
            self.database.execute('DELETE FROM session_admitted WHERE session=? AND account=?',(session,str(account)))
            self.database.execute('DELETE FROM session_priorities WHERE session=? AND account=?',(session,str(account)))

    def attachment(self, account, session, name):
        self._readable(account,session)
        row = self.database.execute('SELECT media,data FROM session_parts WHERE session=? AND name=?',(session,name)).fetchone()
        if not row:
            raise ValueError('attachment absent')
        return row

    def replace_attachment(self, account, session, name, media, body):
        owner = self.database.execute('SELECT owner FROM sessions WHERE id=?',(session,)).fetchone()
        if owner != (str(account),) or name not in ('session-image','changeable-session-data') or len(body)>2*1024*1024:
            raise ValueError('invalid attachment update')
        with self.database:
            self.database.execute('INSERT OR REPLACE INTO session_parts VALUES (?,?,?,?)',(session,name,media,body))

    def member_priority(self, account, session, priority):
        member=self.database.execute('SELECT 1 FROM session_members WHERE session=? AND account=?',(session,str(account))).fetchone()
        if not member or type(priority) is not int or not 0 <= priority <= 0xffffffff:
            raise ValueError('invalid member priority')
        with self.database:
            self.database.execute('INSERT OR REPLACE INTO session_priorities VALUES (?,?,?)',(session,str(account),priority))

    def user_sessions(self, requester, account):
        result=[]
        for (session,) in self.database.execute('SELECT session FROM session_members WHERE account=? ORDER BY session',(str(account),)):
            try:
                row=self.describe(requester,session)
                priority=self.database.execute('SELECT value FROM session_priorities WHERE session=? AND account=?',(session,str(account))).fetchone()
                row.update(index=len(result),priority=priority[0] if priority else 0)
                result.append(row)
            except ValueError:
                pass
        return result

    def close(self):
        self.database.close()
