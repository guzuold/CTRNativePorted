# SPDX-License-Identifier: GPL-2.0-only
import base64
import json
from pathlib import Path
import tempfile
import unittest
from local_matchmaking import Store
from lobby_documents import documents
import room
import demonware_tasks as tasks


class SyncTests(unittest.TestCase):
    def test_host_room_ownership_version_and_commit(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[2]/'runtime/user') as directory:
            store=Store(Path(directory)/'matchmaking.sqlite3');tasks.matchmaking_store=store
            selector=room.token('same')
            plan={'lobby_id':123,'host':42,'players':[{'account':42,'search':{'ruleset_payload':{'filter':{'playlist_id':0}}}},{'account':43}]}
            advertisement={'listen_server':{'local_address':base64.b64encode(bytes(67)).decode(),'security_id':base64.b64encode(bytes(8)).decode(),'security_key':base64.b64encode(bytes(16)).decode()}}
            host,backend=documents(plan,advertisement,100)
            store.database.execute('INSERT INTO lobbies VALUES (?,?,?,?)',('123',selector,'42','[42,43]'))
            store.database.commit();store.save_documents(123,host,backend,1)
            host['attachment']='';host['player_state']['43']=1
            params=[[10,123],[10,1],[16,json.dumps(host)]]
            user={'user_id':42,'room_token':selector}
            wrong={'user_id':43,'room_token':selector}
            self.assertEqual(tasks.Reader(tasks.handle(145,15,params,wrong)).values()[1][1],1)
            wrong={'user_id':42,'room_token':room.token('different')}
            self.assertEqual(tasks.Reader(tasks.handle(145,15,params,wrong)).values()[1][1],1)
            result=tasks.Reader(tasks.handle(145,15,params,user)).values()
            self.assertEqual(result[-1],(1,1))
            self.assertEqual(store.database.execute('SELECT update_id FROM lobby_documents').fetchone(),(2,))
            self.assertEqual(tasks.Reader(tasks.handle(145,15,params,user)).values()[-1],(1,0))
            # Retail clears its attachment to JSON null after starting the race.
            host['attachment']=None;host['update_id']=2
            params=[[10,123],[10,2],[16,json.dumps(host)]]
            self.assertEqual(tasks.Reader(tasks.handle(145,15,params,user)).values()[-1],(1,1))
            self.assertEqual(store.database.execute('SELECT update_id FROM lobby_documents').fetchone(),(3,))
            store.close()


if __name__=='__main__':unittest.main()
