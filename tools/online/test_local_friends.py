# SPDX-License-Identifier: GPL-2.0-only
import json
from pathlib import Path
import tempfile
import unittest
import ctr_online_server as server
from local_friends import Store
from demonware_auth3 import user_id


class FriendsTests(unittest.TestCase):
    def test_empty_graph_route_and_rejections(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[2] / 'runtime/user') as directory:
            store = Store(Path(directory) / 'friends.sqlite3')
            server.friends_store = store
            headers = {'x-ctr-online-id': 'CTRTester1'}
            account = user_id('CTRTester1')
            route = f'/webapi/sdk:userProfile/v1/users/{account}/friendList?limit=500&offset=0'
            status, _, body, event, _ = server.http_answer('GET', route, headers, b'')
            self.assertEqual((status, event), (200, 'webapi_friends'))
            self.assertEqual(json.loads(body), {'totalResults': 0, 'size': 0, 'friendList': []})
            self.assertEqual(server.http_answer('GET', route, {}, b'')[0], 403)
            self.assertEqual(server.http_answer('GET', route + '&unused=1', {'x-ctr-online-id':'CTRTester2'}, b'')[0], 403)
            self.assertEqual(server.http_answer('GET', route.replace('offset=0', 'offset=-1'), headers, b'')[0], 400)
            self.assertEqual(server.http_answer('POST', route, headers, b'')[0], 404)
            store.database.execute('INSERT INTO friends VALUES (?,?)', (str(account), '42'))
            store.database.commit()
            self.assertEqual(server.http_answer('GET', route, headers, b'')[0], 501)
            self.assertEqual(store.page(user_id('CTRTester2'), 0, 500)['totalResults'], 0)
            store.close()
            reopened = Store(Path(directory) / 'friends.sqlite3')
            with self.assertRaises(NotImplementedError):
                reopened.page(account, 0, 500)
            reopened.close()


if __name__ == '__main__':
    unittest.main()
