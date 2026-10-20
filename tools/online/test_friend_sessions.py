# SPDX-License-Identifier: GPL-2.0-or-later
"""Synthetic lifecycle regressions; not proof of retail joinability."""
import json
from contextlib import ExitStack
from pathlib import Path
import tempfile
import unittest
import demonware_tasks as tasks
from local_matchmaking import Store
from local_sessions import Store as SessionStore
from test_local_sessions import fixture
import room


class FriendSessionsTests(unittest.TestCase):
    def test_name_lookup_wire_reply(self):
        reply = tasks.Reader(tasks.handle(12,10,[[8,1],[16,'FriendA']],{})).values()
        self.assertEqual(reply[1:],[(8,0),(3,10),(8,1),(8,1),
                                   (10,tasks.demonware_auth3.user_id('FriendA')),(16,'FriendA')])
        for bad in ([],[[8,2],[16,'FriendA']],[[8,1],[16,'']],[[8,1],[10,42]]):
            self.assertNotEqual(tasks.Reader(tasks.handle(12,10,bad,{})).values()[1][1],0)

    def test_delta_lifecycle_and_wire_reply(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[2]/'runtime/user') as directory, ExitStack() as cleanup:
            path = Path(directory)/'delta.sqlite3'
            store = Store(path)
            cleanup.callback(store.close)
            previous = tasks.matchmaking_store
            tasks.matchmaking_store = store
            self.addCleanup(setattr,tasks,'matchmaking_store',previous)
            selector = room.token('fixture')
            owner = {'user_id':42,'room_token':selector}
            lookup = [[16,'Delta'],[10,42]]
            def call(task, params, user=owner):
                return tasks.Reader(tasks.handle(138,task,params,user)).values()
            reservation = call(19,[[16,'Delta']])
            session = reservation[-1][1]
            self.assertEqual(reservation[1][1],0)
            self.assertEqual(store.friend_sessions('Delta',[42],43),[])
            custom = bytes(tasks.Writer().blob(bytes(8)).blob(bytes(16)).string('session').data)
            info = [[19,bytes(67).hex()],[8,8],[8,8],[19,custom.hex()]]
            identity = [[16,'Delta'],[19,session]]
            self.assertNotEqual(call(20,identity+info,{'user_id':43})[1][1],0)
            self.assertEqual(store.friend_sessions('Delta',[42],43),[])
            self.assertEqual(call(20,identity+info)[1][1],0)
            self.assertEqual(call(12,identity+[[8,2]]+info)[1][1],0)
            reply = call(14,lookup,{'user_id':43})
            self.assertEqual(reply[1:],[(8,0),(3,14),(8,1),(8,1),
                                      (19,bytes(67).hex()),(19,session),(8,8),(8,8),(8,2),
                                      (19,bytes(8).hex()),(19,bytes(16).hex()),(16,'session')])
            self.assertEqual(store.friend_sessions('Delta',[42],42),[])
            bad = identity+[[8,3]]+info+info+info
            self.assertNotEqual(call(12,bad)[1][1],0)
            self.assertEqual(store.friend_sessions('Delta',[42],43)[0][4],2)
            self.assertNotEqual(call(3,identity,{'user_id':43})[1][1],0)
            self.assertEqual(call(3,identity)[1][1],0)
            self.assertEqual(store.friend_sessions('Delta',[42],43),[])
            self.assertNotEqual(call(19,[[16,'Delta']],{'user_id':42,'room_token':''})[1][1],0)

    def test_session_privacy_atomic_owner_update(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[2]/'runtime/user') as directory, ExitStack() as cleanup:
            path = Path(directory)/'sessions.sqlite3'
            store = SessionStore(path)
            cleanup.callback(store.close)
            metadata = {'sessionPrivacy':'private','sessionMaxUser':8,'sessionName':'synthetic','availablePlatforms':['PS4']}
            session = store.create(42,'multipart/mixed; boundary=test',fixture(metadata))['sessionId']
            created = json.loads(store.database.execute('SELECT metadata FROM sessions WHERE id=?',(session,)).fetchone()[0])
            self.assertFalse(created['sessionLockFlag'])
            self.assertGreater(created['sessionCreateTimestamp'],0)
            store.update(42,session,b'{"sessionPrivacy":"public"}')
            for account,change in [(43,b'{"sessionPrivacy":"private"}'),
                                   (42,b'{"sessionPrivacy":"private","unknown":true}'),
                                   (42,b'{"sessionMaxUser":true}'),
                                   (42,b'{}')]:
                with self.assertRaises(ValueError):
                    store.update(account,session,change)
            store.close()
            store = SessionStore(path)
            cleanup.callback(store.close)
            saved = json.loads(store.database.execute('SELECT metadata FROM sessions WHERE id=?',(session,)).fetchone()[0])
            self.assertEqual(saved,{**created,'sessionPrivacy':'public'})


if __name__ == '__main__':
    unittest.main()
