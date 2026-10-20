# SPDX-License-Identifier: GPL-2.0-or-later
"""Durable player advertisements for the project's matchmaking service.

Only the observed retail player-info schema is accepted. Search and peer pairing
remain separate operations; registering an advertisement does not create a lobby.
"""
import base64
import json
import os
import secrets
import sqlite3
import room
import time


class Store:
    def __init__(self, path):
        self.database = sqlite3.connect(path)
        self.database.execute('CREATE TABLE IF NOT EXISTS player_info ('
                              'account TEXT PRIMARY KEY, document TEXT NOT NULL)')
        self.database.execute('CREATE TABLE IF NOT EXISTS player_tokens ('
                              'account TEXT PRIMARY KEY, token TEXT UNIQUE NOT NULL)')
        self.database.execute('CREATE TABLE IF NOT EXISTS player_rooms ('
                              'account TEXT PRIMARY KEY, selector TEXT NOT NULL)')
        self.database.execute('CREATE TABLE IF NOT EXISTS match_sessions ('
                              'id BLOB PRIMARY KEY, selector TEXT NOT NULL, owner TEXT NOT NULL, '
                              'context TEXT NOT NULL, address BLOB NOT NULL, field_a INTEGER NOT NULL, '
                              'field_b INTEGER NOT NULL, custom BLOB NOT NULL)')
        self.database.execute('CREATE TABLE IF NOT EXISTS match_session_players ('
                              'session BLOB NOT NULL, position INTEGER NOT NULL, address BLOB NOT NULL, '
                              'field_a INTEGER NOT NULL, field_b INTEGER NOT NULL, custom BLOB NOT NULL, '
                              'PRIMARY KEY(session,position))')
        columns = {row[1] for row in self.database.execute('PRAGMA table_info(match_sessions)')}
        for name, default in (('players', 1), ('reserved', 0)):
            if name not in columns:
                self.database.execute(f'ALTER TABLE match_sessions ADD COLUMN {name} INTEGER NOT NULL DEFAULT {default}')
        self.database.execute('CREATE TABLE IF NOT EXISTS searches ('
                              'id TEXT PRIMARY KEY, account TEXT NOT NULL, selector TEXT NOT NULL, '
                              'compatibility TEXT NOT NULL, document TEXT NOT NULL, '
                              'status TEXT NOT NULL, lobby TEXT)')
        self.database.execute('CREATE TABLE IF NOT EXISTS lobbies ('
                              'id TEXT PRIMARY KEY, selector TEXT NOT NULL, host TEXT NOT NULL, '
                              'members TEXT NOT NULL)')
        self.database.execute('CREATE TABLE IF NOT EXISTS lobby_documents ('
                              'lobby TEXT PRIMARY KEY, host_document TEXT NOT NULL, '
                              'backend_document TEXT NOT NULL, update_id INTEGER NOT NULL)')
        self.database.commit()

    def advertise(self, params, account, selector):
        if not room.valid_token(selector):
            raise ValueError('valid room selector required')
        if len(params) != 1 or params[0][0] != 16 or not isinstance(params[0][1], str):
            raise ValueError('expected one player-info string')
        text = params[0][1]
        if len(text.encode('utf8')) > 65535:
            raise ValueError('oversize player-info document')
        document = json.loads(text)
        if not isinstance(document, dict) or set(document) != {'listen_server', 'dedicated_server'}:
            raise ValueError('unsupported player-info schema')
        listen = document['listen_server']
        if not isinstance(listen, dict) or set(listen) != {'local_address','security_id','security_key','nat'}:
            raise ValueError('unsupported listen-server schema')
        for key, size in [('local_address',67), ('security_id',8), ('security_key',16)]:
            if not isinstance(listen[key], str) or len(base64.b64decode(listen[key], validate=True)) != size:
                raise ValueError('invalid player-info encoding')
        if type(listen['nat']) is not int or listen['nat'] not in (1,2,3):
            raise ValueError('unsupported NAT value')
        dedicated = document['dedicated_server']
        if not isinstance(dedicated, dict) or set(dedicated) != {'build_name'} or not isinstance(dedicated['build_name'], str):
            raise ValueError('unsupported dedicated-server schema')
        with self.database:
            self.database.execute('INSERT INTO player_info VALUES (?,?) ON CONFLICT(account) '
                                  'DO UPDATE SET document=excluded.document', (str(account), text))
            self.database.execute('INSERT INTO player_rooms VALUES (?,?) ON CONFLICT(account) '
                                  'DO UPDATE SET selector=excluded.selector', (str(account), selector))

    def player_token(self, account):
        if not self.database.execute('SELECT 1 FROM player_info WHERE account=?', (str(account),)).fetchone():
            raise ValueError('player advertisement required')
        row = self.database.execute('SELECT token FROM player_tokens WHERE account=?', (str(account),)).fetchone()
        if row:
            return int(row[0])
        with self.database:
            while True:
                token = secrets.randbits(64)
                if token and not self.database.execute('SELECT 1 FROM player_tokens WHERE token=?', (str(token),)).fetchone():
                    self.database.execute('INSERT INTO player_tokens VALUES (?,?)', (str(account), str(token)))
                    return token

    def close(self):
        self.database.close()

    def register_session(self, params, account, selector):
        if not room.valid_token(selector) or len(params) != 5 or [p[0] for p in params] != [16,19,8,8,19]:
            raise ValueError('unsupported session registration schema')
        context,address,a,b,custom = [p[1] for p in params]
        if not isinstance(context,str) or not 1 <= len(context.encode('utf8')) <= 16:
            raise ValueError('invalid session context')
        address, custom = bytes.fromhex(address), bytes.fromhex(custom)
        if len(address) != 67 or not 1 <= len(custom) <= 65535 or any(type(v) is not int or not 0 <= v <= 0xffffffff for v in (a,b)):
            raise ValueError('invalid session fields')
        with self.database:
            self.database.execute('DELETE FROM match_session_players WHERE session IN '
                                  '(SELECT id FROM match_sessions WHERE owner=? AND context=?)', (str(account), context))
            self.database.execute('DELETE FROM match_sessions WHERE owner=? AND context=?', (str(account), context))
            while True:
                session = secrets.token_bytes(8)
                if any(session) and not self.database.execute('SELECT 1 FROM match_sessions WHERE id=?',(session,)).fetchone():
                    self.database.execute('INSERT INTO match_sessions VALUES (?,?,?,?,?,?,?,?,?,?)',
                                          (session,selector,str(account),context,address,a,b,custom,1,0))
                    return session

    def update_session_players(self, params, account, selector):
        if not room.valid_token(selector) or len(params) < 3 or [p[0] for p in params[:3]] != [16,19,8]:
            raise ValueError('invalid session player header')
        context,session,count = [p[1] for p in params[:3]]
        session = bytes.fromhex(session)
        if type(count) is not int or not 1 <= count <= 8 or len(params) != 7 or len(session) != 8:
            raise ValueError('invalid session player count')
        if not self.database.execute('SELECT 1 FROM match_sessions WHERE id=? AND owner=? '
                                     'AND selector=? AND context=?',
                                     (session,str(account),selector,context)).fetchone():
            raise ValueError('session owner or room mismatch')
        address,a,b,custom = self._session_info(params[3:])
        with self.database:
            self.database.execute('DELETE FROM match_session_players WHERE session=?',(session,))
            self.database.execute('UPDATE match_sessions SET address=?,field_a=?,field_b=?,custom=?,players=? WHERE id=?',
                                  (address,a,b,custom,count,session))

    @staticmethod
    def _session_info(params):
        if len(params) != 4 or [p[0] for p in params] != [19,8,8,19]:
            raise ValueError('invalid session fields')
        address,a,b,custom = [p[1] for p in params]
        address,custom = bytes.fromhex(address),bytes.fromhex(custom)
        if len(address) != 67 or not 1 <= len(custom) <= 65535 or any(
                type(v) is not int or not 0 <= v <= 0xffffffff for v in (a,b)):
            raise ValueError('invalid session values')
        return address,a,b,custom

    def reserve_session(self, params, account, selector):
        if not room.valid_token(selector) or len(params) != 1 or params[0][0] != 16:
            raise ValueError('invalid session reservation')
        context = params[0][1]
        if not isinstance(context,str) or not 1 <= len(context.encode('utf8')) <= 16:
            raise ValueError('invalid session context')
        with self.database:
            self.database.execute('DELETE FROM match_sessions WHERE owner=? AND context=? AND reserved=1',
                                  (str(account),context))
            while True:
                session = secrets.token_bytes(8)
                if any(session) and not self.database.execute('SELECT 1 FROM match_sessions WHERE id=?',(session,)).fetchone():
                    self.database.execute('INSERT INTO match_sessions VALUES (?,?,?,?,?,?,?,?,?,?)',
                                          (session,selector,str(account),context,b'',0,0,b'',0,1))
                    return session

    def _owned_session(self, params, account):
        if len(params) != 2 or [p[0] for p in params] != [16,19]:
            raise ValueError('invalid session identity')
        context,session = params[0][1],bytes.fromhex(params[1][1])
        if not self.database.execute('SELECT 1 FROM match_sessions WHERE id=? AND context=? AND owner=?',
                                     (session,context,str(account))).fetchone():
            raise ValueError('unknown session or not its owner')
        return session

    def update_session(self, params, account, initialize=False):
        if len(params) != 6:
            raise ValueError('invalid session update')
        session = self._owned_session(params[:2], account)
        address,a,b,custom = self._session_info(params[2:])
        with self.database:
            self.database.execute('UPDATE match_sessions SET address=?,field_a=?,field_b=?,custom=? WHERE id=?',
                                  (address,a,b,custom,session))
            if initialize:
                self.database.execute('UPDATE match_sessions SET reserved=0,players=1 WHERE id=?',(session,))

    def delete_session(self, params, account):
        session = self._owned_session(params,account)
        with self.database:
            self.database.execute('DELETE FROM match_session_players WHERE session=?',(session,))
            self.database.execute('DELETE FROM match_sessions WHERE id=?',(session,))

    def friend_sessions(self, context, accounts, requester):
        result = []
        for account in accounts:
            if account == requester:
                continue
            result.extend(self.database.execute(
                'SELECT address,id,field_a,field_b,players,custom FROM match_sessions '
                'WHERE owner=? AND context=? AND reserved=0 ORDER BY id',(str(account),context)).fetchall())
        return result

    def room_players(self, account, selector):
        if not room.valid_token(selector) or not self.database.execute(
                'SELECT 1 FROM player_rooms WHERE account=? AND selector=?',
                (str(account),selector)).fetchone():
            raise ValueError('player is not registered in this room')
        return self.database.execute('SELECT r.account,t.token,p.document FROM player_rooms r '
                                     'JOIN player_tokens t ON t.account=r.account '
                                     'JOIN player_info p ON p.account=r.account '
                                     'WHERE r.selector=? ORDER BY r.account', (selector,)).fetchall()

    def start_search(self, params, account, selector):
        if len(params) != 1 or params[0][0] != 16:
            raise ValueError('expected one search document')
        document = json.loads(params[0][1])
        if not isinstance(document,dict) or set(document) != {'members','tokens','lobby_slots','dedicated_server','team_balance','ruleset_payload','player_id_to_slots','performance'}:
            raise ValueError('unsupported search schema')
        # First verified scope: one local player per instance, no guests or teams.
        if document['members'] != [account] or document['tokens'] != [self.player_token(account)]:
            raise ValueError('search member or token mismatch')
        if not room.valid_token(selector) or not self.database.execute(
                'SELECT 1 FROM player_rooms WHERE account=? AND selector=?', (str(account),selector)).fetchone():
            raise ValueError('search room mismatch')
        slots = document['lobby_slots']
        if not isinstance(slots,dict) or set(slots) != {'min','max'} or any(type(slots[k]) is not int for k in slots) or not 2 <= slots['min'] <= slots['max'] <= 8:
            raise ValueError('unsupported lobby slots')
        if document['dedicated_server'] != {'request_level':'not_needed'} or document['team_balance'] != {'num_teams':0} or document['player_id_to_slots'] != {str(account):0}:
            raise ValueError('unsupported dedicated server, team or guest schema')
        filters = document['ruleset_payload']
        performance = document['performance']
        if not isinstance(filters,dict) or set(filters) != {'filter'} or not isinstance(filters['filter'],dict) or set(filters['filter']) != {'matchmaking_version','playlist_id'} or any(type(v) is not int or v < 0 for v in filters['filter'].values()) or (not isinstance(performance,dict) or set(performance) != {'game_type'} or type(performance['game_type']) is not int or not 0 <= performance['game_type'] <= 0xffffffff):
            raise ValueError('unsupported search filters')
        compatibility = json.dumps([slots,filters,performance],sort_keys=True,separators=(',',':'))
        text=json.dumps(document,separators=(',',':'))
        previous=self.database.execute('SELECT id FROM searches WHERE account=? AND selector=? '
                                       'AND document=? AND status=?', (str(account),selector,text,'pending')).fetchone()
        if previous:
            return int(previous[0])
        with self.database:
            self.database.execute('UPDATE searches SET status=? WHERE account=? AND status=?',('cancelled',str(account),'pending'))
            while True:
                search=secrets.randbits(52)
                if search and not self.database.execute('SELECT 1 FROM searches WHERE id=?',(str(search),)).fetchone():
                    self.database.execute('INSERT INTO searches VALUES (?,?,?,?,?,?,NULL)',
                                          (str(search),str(account),selector,compatibility,text,'pending'))
                    return search

    def match_ready(self, available_accounts):
        """Select only connected pending players with equal room and retail filters."""
        rows=self.database.execute('SELECT id,account,selector,compatibility,document FROM searches '
                                   'WHERE status=? ORDER BY rowid',('pending',)).fetchall()
        groups={}
        for row in rows:
            if available_accounts.get(int(row[1])) == row[2]:
                groups.setdefault((row[2],row[3]),[]).append(row)
        plans=[]
        with self.database:
            for (selector,_),players in groups.items():
                slots=json.loads(players[0][4])['lobby_slots']
                min_players = slots['min']
                env_min = os.environ.get('CTR_ONLINE_MIN_PLAYERS')
                if env_min:
                    try:
                        min_players = max(min_players, int(env_min))
                    except ValueError:
                        pass
                if len(players) < min_players:
                    continue
                players=players[:slots['max']]
                while True:
                    lobby=secrets.randbits(52)
                    if lobby and not self.database.execute('SELECT 1 FROM lobbies WHERE id=?',(str(lobby),)).fetchone():
                        break
                members=[int(p[1]) for p in players]
                self.database.execute('INSERT INTO lobbies VALUES (?,?,?,?)',(str(lobby),selector,str(members[0]),json.dumps(members)))
                for player in players:
                    self.database.execute('UPDATE searches SET status=?,lobby=? WHERE id=?',('paired',str(lobby),player[0]))
                plans.append({'lobby_id':lobby,'selector':selector,'host':members[0],
                              'players':[{'account':int(p[1]),'search_id':int(p[0]),'search':json.loads(p[4])} for p in players]})
        return plans

    def save_documents(self, lobby, host, backend, update_id):
        with self.database:
            self.database.execute('INSERT INTO lobby_documents VALUES (?,?,?,?) ON CONFLICT(lobby) '
                                  'DO UPDATE SET host_document=excluded.host_document, '
                                  'backend_document=excluded.backend_document, update_id=excluded.update_id',
                                  (str(lobby),json.dumps(host,separators=(',',':')),
                                   json.dumps(backend,separators=(',',':')),update_id))

    def sync_document(self, params, account, selector):
        if len(params) != 3 or [p[0] for p in params] != [10,10,16]:
            raise ValueError('invalid lobby synchronization fields')
        lobby,expected,text=[p[1] for p in params]
        row=self.database.execute('SELECT members FROM lobbies WHERE id=? AND host=? AND selector=?',
                                  (str(lobby),str(account),selector)).fetchone()
        current=self.database.execute('SELECT backend_document,update_id FROM lobby_documents WHERE lobby=?',(str(lobby),)).fetchone()
        if not row or not current:
            raise ValueError('lobby host or room mismatch')
        if current[1] != expected:
            return False
        document=json.loads(text)
        keys={'update_time','update_id','lobby_open','lobby_open_for_pres_join','player_state','listen_server','team_balance','ruleset_payload','attachment'}
        if not isinstance(document,dict) or not keys <= set(document) or not set(document) <= keys|{'game_id'}:
            raise ValueError('unsupported host document schema')
        if document['update_id'] != expected or type(document['update_time']) is not int or document['update_time'] < 0 or any(type(document[k]) is not bool for k in ('lobby_open','lobby_open_for_pres_join')):
            raise ValueError('invalid host document version or flags')
        members={str(member) for member in json.loads(row[0])}
        state=document['player_state']
        if not isinstance(state,dict) or set(state) != members or any(type(v) is not int or not 0 <= v < 4 for v in state.values()):
            raise ValueError('invalid lobby player states')
        listen=document['listen_server']
        if not isinstance(listen,dict) or set(listen) != {'host_address','host_player_id'} or listen['host_player_id'] != account or len(base64.b64decode(listen['host_address'],validate=True)) != 67:
            raise ValueError('invalid lobby host advertisement')
        if document['attachment'] is not None and (not isinstance(document['attachment'],str) or len(document['attachment'].encode()) >= 1024):
            raise ValueError('invalid lobby attachment')
        if not isinstance(document['team_balance'],dict) or set(document['team_balance']) != {'can_change_teams'} or type(document['team_balance']['can_change_teams']) is not bool:
            raise ValueError('invalid lobby team balance')
        rules=document['ruleset_payload']
        if not isinstance(rules,dict) or not set(rules) <= {'is_merge_src','is_merge_dst'} or any(type(v) is not bool for v in rules.values()):
            raise ValueError('unsupported lobby merge rules')
        backend=json.loads(current[0])
        version=expected+1
        document['update_id']=version;document['update_time']=int(time.time())
        backend['update_id']=version;backend['update_time']=document['update_time']
        self.save_documents(lobby,document,backend,version)
        return True
