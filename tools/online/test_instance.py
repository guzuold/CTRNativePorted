# SPDX-License-Identifier: GPL-2.0-only
"""Real directory/junction and exclusive-lock checks; state remains under runtime/user."""
import unittest
from pathlib import Path
from instance import prepare, lock


class InstanceTests(unittest.TestCase):
    def test_names_cannot_escape(self):
        root = Path(__file__).resolve().parents[2]
        for name in ('', '..', '../outside', 'a/b', 'a b', 'a' * 33):
            with self.assertRaises(ValueError): prepare(root, name)

    def test_distinct_roots_and_lock_collision(self):
        root = Path(__file__).resolve().parents[2]
        a = prepare(root, 'r8-isolation-control-a')
        b = prepare(root, 'r8-isolation-control-b')
        self.assertNotEqual((a / 'runtime/user').resolve(), (b / 'runtime/user').resolve())
        self.assertEqual((a / 'build').resolve(), (root / 'build').resolve())
        with lock(a), lock(b):
            with self.assertRaises(FileExistsError):
                with lock(a): pass
        self.assertFalse((a / 'instance.lock').exists())
        self.assertFalse((b / 'instance.lock').exists())


if __name__ == '__main__': unittest.main()
