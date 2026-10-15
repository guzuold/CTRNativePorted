# SPDX-License-Identifier: GPL-2.0-or-later
"""Pit Stop and Wumpa Challenge server tasks against the exact retail v01.21 request bytes
recorded in runtime/user/logs/online_server_20261015_161036.jsonl; synthetic accounts."""
import tempfile
import unittest
from pathlib import Path
import demonware_rest
import demonware_tasks as tasks
import local_achievements
import local_marketplace

INVENTORY = bytes.fromhex('5003a5106f6374616e655f70736e000801000000085803000000')
BALANCES = bytes.fromhex('5003f517080e0000000a0a6f6374616e655f70736e100300')
LOGIN = bytes.fromhex('7d030117082e0000000a0a6f6374616e655f70736e129b8080000a056c6f67696e10f6bac3d606'
                      '1a878080000a03656e76100220001a0000')
STATES = [bytes.fromhex('7d03031708120000000a0a6f6374616e655f70736e10%02x1864220000' % kind) for kind in range(8)]
USER = {'user_id': 42}


def header(reply):
    """(error, task) of a typed reply header."""
    return int.from_bytes(reply[10:14], 'little'), reply[15]


class PitStopTasks(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[2] / 'runtime/user')
        root = Path(self.directory.name)
        tasks.marketplace_store = local_marketplace.Store(root / 'marketplace.sqlite3')
        tasks.achievement_store = local_achievements.Store(root / 'achievements.sqlite3', demonware_rest.fields)

    def tearDown(self):
        tasks.marketplace_store.close()
        tasks.achievement_store.close()
        tasks.marketplace_store = tasks.achievement_store = None
        self.directory.cleanup()

    def answer(self, request, user=USER):
        return tasks.handle(*tasks.parse_request(request), user)

    def test_retail_requests_are_answered(self):
        reply = self.answer(INVENTORY)
        self.assertEqual(header(reply), (0, 165))
        self.assertEqual(reply[16:], bytes([8, 0, 0, 0, 0]))  # no inventory results
        reply = self.answer(BALANCES)
        self.assertEqual(header(reply), (0, 245))
        self.assertEqual(reply[16:], bytes([23, 8, 0, 0, 0, 0, 0]))  # empty struct buffer
        self.assertEqual(header(self.answer(LOGIN)), (0, 1))
        for request in STATES:
            reply = self.answer(request)
            self.assertEqual(header(reply), (0, 3))
            self.assertEqual(reply[16:], bytes([23, 8, 0, 0, 0, 0, 0]))
        events = tasks.achievement_store.database.execute('SELECT account, event FROM events').fetchall()
        self.assertEqual(events, [('42', '{"name": "login", "time": 1792073078}')])

    def test_balances_are_serialized_per_account(self):
        database = tasks.marketplace_store.database
        database.executemany('INSERT INTO balances VALUES (?, ?, ?)', [('42', 1, 500), ('42', 2, -3), ('43', 1, 9)])
        reply = self.answer(BALANCES)
        message = reply[16 + 6:-1]
        entries = [demonware_rest.fields(v) for t, w, v in demonware_rest.fields(message)]
        self.assertEqual(entries, [[(1, 0, 1), (2, 0, 1000)], [(1, 0, 2), (2, 0, 5)]])  # zigzag

    def test_unsupported_requests_fail_explicitly(self):
        tasks.marketplace_store.database.execute("INSERT INTO inventory VALUES ('42', 7, 1)")
        self.assertEqual(header(self.answer(INVENTORY))[0], tasks.UNSERVED_ERROR)  # layout not validated
        self.assertEqual(header(self.answer(INVENTORY, {'user_id': 43}))[0], 0)  # other account unaffected
        wrong_context = BALANCES.replace(b'octane_psn', b'octane_xbx')
        self.assertEqual(header(self.answer(wrong_context))[0], tasks.UNSERVED_ERROR)
        kind_eight = bytes.fromhex('7d03031708120000000a0a6f6374616e655f70736e10081864220000')
        self.assertEqual(header(self.answer(kind_eight))[0], tasks.UNSERVED_ERROR)
        paged = bytes.fromhex('7d03031708130000000a0a6f6374616e655f70736e100018642201780000')
        self.assertEqual(header(self.answer(paged))[0], tasks.UNSERVED_ERROR)


if __name__ == '__main__':
    unittest.main()
