# SPDX-License-Identifier: GPL-2.0-or-later
"""Project lobby documents and observed typed notification formats.

The host's actual advertisement supplies the address and security material.
Guests remain unacknowledged until the original host updates the document.
"""
import json
import time
from demonware_tasks import Writer

ROOM_CREATED_EVENT = 2005
ROOM_JOIN_EVENT = 2001
ROOM_UPDATED_EVENT = 2006
PUSH_RECORD = 2


def documents(plan, advertisement, now=None):
    now=int(time.time()) if now is None else now
    listen=advertisement['listen_server']
    members=[p['account'] for p in plan['players']]
    search=plan['players'][0]['search']
    host={'update_time':now,'update_id':1,'lobby_open':True,'lobby_open_for_pres_join':False,
          'player_state':{str(account):0 for account in members},
          'listen_server':{'host_address':listen['local_address'],'host_player_id':plan['host']},
          'team_balance':{'can_change_teams':False},'ruleset_payload':{},'attachment':None}
    backend={'update_id':1,'update_time':now,'lobby_id':plan['lobby_id'],'create_time':now,
             'players':{str(account):'expected' for account in members},
             'dtls':{'security_id':listen['security_id'],'security_key':listen['security_key']},
             'ruleset_payload':search['ruleset_payload']}
    return host,backend


def create_event(search_id,lobby,host,backend):
    writer=Writer().u32(ROOM_CREATED_EVENT).u64(search_id).u64(lobby).u64(host['update_id'])
    writer.string(json.dumps(host,separators=(',',':'))).string(json.dumps(backend,separators=(',',':')))
    return bytes(writer.data)


def join_event(search_id,lobby,host,backend):
    writer=Writer().u32(ROOM_JOIN_EVENT).u64(search_id).u64(lobby)
    writer.string(json.dumps(host,separators=(',',':'))).string(json.dumps(backend,separators=(',',':')))
    return bytes(writer.data)


def update_event(lobby,host,backend):
    writer=Writer().u32(ROOM_UPDATED_EVENT).u64(lobby).u64(host['update_id'])
    writer.string(json.dumps(host,separators=(',',':'))).string(json.dumps(backend,separators=(',',':')))
    return bytes(writer.data)
