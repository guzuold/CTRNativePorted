# SPDX-License-Identifier: GPL-2.0-only
import base64
import json
from pathlib import Path
import tempfile
import unittest
from local_matchmaking import Store
import demonware_tasks as tasks
import room


class MatchmakingTests(unittest.TestCase):
    def test_advertisement_durable_and_isolated(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[2] / 'runtime/user') as directory:
            path = Path(directory) / 'matchmaking.sqlite3'
            store = Store(path)
            with self.assertRaises(ValueError):
                store.player_token(42)
            document = {'listen_server': {key:base64.b64encode(bytes(size)).decode() for key,size in [('local_address',67),('security_id',8),('security_key',16)]}, 'dedicated_server':{'build_name':'synthetic'}}
            document['listen_server']['nat'] = 1
            params = [[16,json.dumps(document)]]
            tasks.matchmaking_store = store
            selector = room.token('synthetic room')
            user = {'user_id':42,'room_token':selector}
            result = tasks.Reader(tasks.handle(145,2,params,user)).values()
            self.assertEqual([r[1] for r in result[1:]], [0,2,0])
            document['listen_server']['nat'] = 2
            store.advertise([[16,json.dumps(document)]],43,selector)
            token = store.player_token(42)
            self.assertNotEqual(token, store.player_token(43))
            token_reply = tasks.Reader(tasks.handle(145,3,[],user)).values()
            self.assertEqual([r[1] for r in token_reply[1:]], [0,3,1,1,token])
            for bad in ([], [[8,1]], [[16,'{}']], [[16,'null']], [[16,'{']]):
                self.assertEqual(tasks.Reader(tasks.handle(145,2,bad,user)).values()[1][1], 1)
            other = room.token('different room')
            store.advertise(params,44,other)
            store.player_token(44)
            self.assertEqual([r[0] for r in store.room_players(42,selector)], ['42','43'])
            self.assertEqual([r[0] for r in store.room_players(44,other)], ['44'])
            with self.assertRaises(ValueError):
                store.room_players(42,other)
            store.close()
            store = Store(path)
            rows = store.database.execute('SELECT account,document FROM player_info ORDER BY account').fetchall()
            self.assertEqual([r[0] for r in rows], ['42','43','44'])
            self.assertEqual([json.loads(r[1])['listen_server']['nat'] for r in rows], [1,2,1])
            self.assertEqual(store.player_token(42), token)
            store.close()


if __name__ == '__main__':
    unittest.main()
