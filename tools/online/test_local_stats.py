# SPDX-License-Identifier: GPL-2.0-only
"""Persistence and rejection controls; synthetic accounts/ratings."""
import tempfile
import unittest
from pathlib import Path
from local_stats import Store


class StatisticsTests(unittest.TestCase):
    def test_persistence_replacement_and_isolation(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[2] / 'runtime/user') as directory:
            path = Path(directory) / 'stats.sqlite3'
            store = Store(path)
            store.write([[8,1000],[10,42],[3,0],[9,-1]], 42)
            store.write([[8,1000],[10,43],[3,0],[9,7]], 43)
            store.write([[8,1000],[10,42],[3,0],[9,5]], 42)
            store.close()
            store = Store(path)
            self.assertEqual(store.database.execute('SELECT account,rating FROM ratings ORDER BY account').fetchall(), [('42',5),('43',7)])
            for params in ([[8,1000],[10,42],[3,1],[9,9]], [[8,1000],[10,99],[3,0],[9,9]], [[8,1000]]):
                with self.assertRaises(ValueError):
                    store.write(params, 42)
            self.assertEqual(store.database.execute('SELECT COUNT(*) FROM ratings').fetchone()[0], 2)
            store.close()


if __name__ == '__main__':
    unittest.main()
