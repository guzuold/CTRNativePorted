# SPDX-License-Identifier: GPL-2.0-only
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from tools.online.local_backup import backup_savedata


class LocalBackupTests(unittest.TestCase):
    def test_save_versions_and_missing_source(self):
        with tempfile.TemporaryDirectory() as directory:
            user = Path(directory)
            self.assertIsNone(backup_savedata(user, 'before_launch'))
            source = user / 'savedata/profile/save.bin'
            source.parent.mkdir(parents=True)
            source.write_bytes(b'\x00old\xff')
            before = backup_savedata(user, 'before_launch')
            source.write_bytes(b'\x00new\xff')
            after = backup_savedata(user, 'after_exit')
            self.assertEqual((before / 'profile/save.bin').read_bytes(), b'\x00old\xff')
            self.assertEqual((after / 'profile/save.bin').read_bytes(), source.read_bytes())
            self.assertNotEqual(before, after)

    def test_failed_copy_is_not_published(self):
        with tempfile.TemporaryDirectory() as directory:
            user = Path(directory)
            (user / 'savedata').mkdir()
            with patch('tools.online.local_backup.shutil.copytree', side_effect=OSError('disk full')):
                with self.assertRaises(OSError):
                    backup_savedata(user, 'before_launch')
            self.assertFalse(list((user / 'savedata_backups').glob('*')))


if __name__ == '__main__':
    unittest.main()
