# SPDX-License-Identifier: GPL-2.0-only
import base64
import json
from pathlib import Path
import tempfile
import unittest
import room
from local_matchmaking import Store
import demonware_tasks as tasks


class RoomSearchTests(unittest.TestCase):
    def test_same_password_only_and_original_search_shape(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[2]/'runtime/user') as directory:
            store=Store(Path(directory)/'matchmaking.sqlite3');tasks.matchmaking_store=store
            first,other=room.token('same'),room.token('different')
            info={'listen_server':{'local_address':base64.b64encode(bytes(67)).decode(),
                  'security_id':base64.b64encode(bytes(8)).decode(),'security_key':base64.b64encode(bytes(16)).decode(),'nat':1},'dedicated_server':{'build_name':'synthetic'}}
            searches={}
            for account,selector in [(42,first),(43,first),(44,other)]:
                store.advertise([[16,json.dumps(info)]],account,selector)
                document={'members':[account],'tokens':[store.player_token(account)],'lobby_slots':{'min':2,'max':8},
                          'dedicated_server':{'request_level':'not_needed'},'team_balance':{'num_teams':0},
                          'ruleset_payload':{'filter':{'matchmaking_version':200,'playlist_id':0}},
                          'player_id_to_slots':{str(account):0},'performance':{'game_type':1}}
                searches[account]=[[16,json.dumps(document)]]
            store.start_search(searches[42],42,first)
            store.start_search(searches[44],44,other)
            available={42:first,43:first,44:other}
            self.assertEqual(store.match_ready(available),[])
            with self.assertRaises(ValueError):store.start_search(searches[43],43,other)
            reply=tasks.Reader(tasks.handle(145,18,searches[43],{'user_id':43,'room_token':first})).values()
            self.assertGreater(json.loads(reply[-1][1])['mm_id'],0)
            self.assertEqual(store.match_ready({42:other,43:first,44:other}),[])
            plans=store.match_ready(available)
            self.assertEqual([[p['account'] for p in plan['players']] for plan in plans],[[42,43]])
            self.assertEqual(store.database.execute('SELECT status FROM searches WHERE account=?',('44',)).fetchone(),('pending',))
            self.assertEqual(store.match_ready(available),[])
            store.close()


if __name__ == '__main__':unittest.main()
