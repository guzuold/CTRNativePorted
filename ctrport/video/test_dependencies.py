# SPDX-License-Identifier: GPL-2.0-only
"""Regression checks for reviewed adaptation preservation during regeneration."""
import copy
import json
import unittest
from pathlib import Path
import dependencies

class ReuseManifestControl(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.graph = json.loads((dependencies.ROOT / 'ctrport/video/dependency_inventory.json').read_text())
        cls.previous = json.loads((dependencies.ROOT / 'ctrport/video/reuse_manifest.json').read_text())

    def test_preserve_and_idempotent(self):
        first = dependencies.reuse_manifest(self.graph, self.previous)
        self.assertEqual(first, dependencies.reuse_manifest(self.graph, first))
        old = [r for r in self.previous['files'] if r.get('actually_reused_in_ctr_video')]
        self.assertEqual(old, [r for r in first['files'] if r.get('actually_reused_in_ctr_video')])
        self.assertEqual(first['actually_reused_count'], len(old))
        self.assertEqual(first['file_count'], len(self.graph['files']))
        self.assertEqual(first['entry_count'], len(self.graph['files']) + len(old))

    def test_reject_broken_provenance(self):
        for change in ('hash', 'revision', 'license', 'missing_source', 'missing_destination', 'outside_destination', 'duplicate'):
            with self.subTest(change=change):
                previous = copy.deepcopy(self.previous)
                row = next(r for r in previous['files'] if r.get('actually_reused_in_ctr_video'))
                if change == 'hash': row['sha256'] = '0' * 64
                if change == 'revision': row['revision'] = 'changed'
                if change == 'license': row['spdx'] = ['MIT']
                if change == 'missing_source': row['source'] = 'absent.cpp'
                if change == 'missing_destination': row['destination'] = 'ctrport/video/absent.h'
                if change == 'outside_destination': row['destination'] = 'AGENTS.md'
                if change == 'duplicate': previous['files'].append(copy.deepcopy(row))
                with self.assertRaises(ValueError): dependencies.reuse_manifest(self.graph, previous)

    def test_empty_review_does_not_invent_reuse(self):
        result = dependencies.reuse_manifest(self.graph, {'files': []})
        self.assertEqual(result['actually_reused_count'], 0)
        self.assertTrue(all(r['destination'] is None for r in result['files']))

if __name__ == '__main__': unittest.main()
