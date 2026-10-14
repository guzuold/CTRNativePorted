# SPDX-License-Identifier: GPL-2.0-only
import base64
import json
import unittest
from lobby_documents import documents,create_event,join_event
from demonware_tasks import Reader


class DocumentTests(unittest.TestCase):
    def test_notifications_keep_actual_advertisement(self):
        plan={'lobby_id':123,'host':42,'players':[{'account':42,'search':{'ruleset_payload':{'filter':{'playlist_id':0}}}},{'account':43}]}
        listen={'local_address':base64.b64encode(bytes(range(67))).decode(),
                'security_id':base64.b64encode(bytes(range(8))).decode(),
                'security_key':base64.b64encode(bytes(range(16))).decode()}
        host,backend=documents(plan,{'listen_server':listen},100)
        self.assertEqual(host['listen_server']['host_address'],listen['local_address'])
        self.assertIsNone(host['attachment'])
        self.assertEqual(backend['players'],{'42':'expected','43':'expected'})
        self.assertEqual(backend['dtls']['security_key'],listen['security_key'])
        create=Reader(create_event(5,123,host,backend)).values()
        self.assertEqual([p[1] for p in create[:4]],[2005,5,123,1])
        self.assertEqual(json.loads(create[4][1]),host)
        join=Reader(join_event(6,123,host,backend)).values()
        self.assertEqual([p[1] for p in join[:3]],[2001,6,123])
        self.assertEqual(json.loads(join[4][1]),backend)


if __name__=='__main__':unittest.main()
