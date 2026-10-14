# SPDX-License-Identifier: GPL-2.0-only
import tempfile
from pathlib import Path
import unittest
import demonware_tasks as tasks
from local_matchmaking import Store
import room


class RegistrationTests(unittest.TestCase):
    def test_persist_before_reply_and_invalid_wire(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[2] / 'runtime/user') as directory:
            path=Path(directory)/'matchmaking.sqlite3'
            store=Store(path); tasks.matchmaking_store=store
            user={'user_id':42,'room_token':room.token('synthetic')}
            params=[[16,'Delta'],[19,bytes(67).hex()],[8,8],[8,8],[19,'1234']]
            result=tasks.Reader(tasks.handle(138,1,params,user)).values()
            self.assertEqual([v for _,v in result[1:5]],[0,1,1,1])
            session=bytes.fromhex(result[5][1]);self.assertEqual(len(session),8)
            self.assertEqual(store.database.execute('SELECT owner FROM match_sessions WHERE id=?',(session,)).fetchone(),('42',))
            update=[[16,'Delta'],[19,session.hex()],[8,1]]+params[1:]
            self.assertEqual(tasks.Reader(tasks.handle(138,12,update,user)).values()[1][1],0)
            self.assertEqual(store.database.execute('SELECT custom FROM match_session_players WHERE session=?',(session,)).fetchone(),(b'\x12\x34',))
            for invalid_user in ({'user_id':43,'room_token':user['room_token']},
                                 {'user_id':42,'room_token':room.token('other')}):
                self.assertEqual(tasks.Reader(tasks.handle(138,12,update,invalid_user)).values()[1][1],1)
            for bad in (params[:-1], [[16,'Delta'],[19,'00'],[8,8],[8,8],[19,'1234']]):
                self.assertEqual(tasks.Reader(tasks.handle(138,1,bad,user)).values()[1][1],1)
            store.close();store=Store(path)
            self.assertEqual(store.database.execute('SELECT selector FROM match_sessions WHERE id=?',(session,)).fetchone(),(user['room_token'],))
            store.close()
        for wire in (b'\x08\x01',b'\x13\x08\x10\0\0\0\x01',b'\x13\x03\x01\0'):
            with self.assertRaises(ValueError):
                tasks.Reader(wire).values()


if __name__ == '__main__':
    unittest.main()
