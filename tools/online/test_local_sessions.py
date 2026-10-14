# SPDX-License-Identifier: GPL-2.0-only
import json
from pathlib import Path
import tempfile
import unittest
from local_sessions import Store


def fixture(metadata):
    parts = [('session-request','application/json',json.dumps(metadata).encode()),
             ('session-image','image/jpeg',b'\xff\xd8\x00\xff\xd9'),
             ('session-data','application/octet-stream',b'fixed'),
             ('changeable-session-data','application/octet-stream',b'changeable')]
    return b''.join(b'--test\r\nContent-Type:'+media.encode()+b'\r\nContent-Description:'+name.encode()+
                    b'\r\nContent-Length:'+str(len(data)).encode()+b'\r\n\r\n'+data+b'\r\n\r\n'
                    for name,media,data in parts) + b'--test--'


class SessionTests(unittest.TestCase):
    def test_create_persistence_and_invalid_request(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[2] / 'runtime/user') as directory:
            path = Path(directory) / 'sessions.sqlite3'
            store = Store(path)
            metadata = {'sessionPrivacy':'private','sessionMaxUser':8,'sessionName':'synthetic','availablePlatforms':['PS4']}
            body = fixture(metadata)
            first = store.create(42,'multipart/mixed; boundary=test',body)['sessionId']
            second = store.create(43,'multipart/mixed; boundary=test',body)['sessionId']
            self.assertNotEqual(first,second)
            for content,data in [('application/json',body),('multipart/mixed; boundary=test',body[:-3]),('multipart/mixed; boundary=test',body.replace(b'Content-Description:',b'Other-Description:'))]:
                with self.assertRaises((ValueError,KeyError)):
                    store.create(42,content,data)
            # Observed retail variant ends exactly after the last part's data.
            eof_body = body.removesuffix(b'\r\n\r\n--test--')
            store.create(44,'multipart/mixed; boundary="test"',eof_body)
            with self.assertRaises(ValueError):
                store.create(44,'multipart/mixed; boundary=test',eof_body[:-1])
            store.close()
            store = Store(path)
            self.assertEqual(store.database.execute('SELECT owner FROM sessions ORDER BY owner').fetchall(),[('42',),('43',),('44',)])
            self.assertEqual(store.database.execute('SELECT data FROM session_parts WHERE session=? AND name=?',(first,'session-image')).fetchone()[0],b'\xff\xd8\x00\xff\xd9')
            store.close()


if __name__ == '__main__':
    unittest.main()
