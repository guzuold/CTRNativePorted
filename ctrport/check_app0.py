# SPDX-License-Identifier: GPL-2.0-only
"""Synthetic native resolver checks; no game filesystem implementation credit."""
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / 'build/CTRPort-infra/ctrport.exe'


def main():
    with tempfile.TemporaryDirectory(dir=EXE.parent) as temporary:
        base, patch = Path(temporary) / 'base', Path(temporary) / 'patch'
        base.mkdir(); patch.mkdir()
        (base / 'shared.txt').write_text('base')
        (patch / 'shared.txt').write_text('patch')
        (base / 'fallback.txt').write_text('fallback')
        before = {p: p.read_bytes() for p in Path(temporary).rglob('*.txt')}
        for name, expected in [('shared.txt', patch / 'shared.txt'), ('fallback.txt', base / 'fallback.txt')]:
            p = subprocess.run([str(EXE), '--resolve-app0', str(base), str(patch), name], capture_output=True, text=True)
            assert p.returncode == 0 and Path(p.stdout.strip()) == expected.resolve()
        for invalid in ['../shared.txt', '/shared.txt', 'C:/shared.txt', 'shared.txt:stream', 'dir\\shared.txt', '.', 'missing.txt']:
            p = subprocess.run([str(EXE), '--resolve-app0', str(base), str(patch), invalid], capture_output=True, text=True)
            assert p.returncode == 4 and p.stderr.startswith('CTRPort app0:')
        assert all(p.read_bytes() == content for p, content in before.items())
    report = {'passed': True, 'positive_cases': 2, 'negative_cases': 7, 'inputs_unchanged': True,
              'exe_sha256': hashlib.sha256(EXE.read_bytes()).hexdigest(),
              'scope': 'synthetic native readonly path resolution; no guest syscalls implemented',
              'runtime_loaded': False, 'main_reached': False, 'runtime_replacements': 0}
    (ROOT / 'reports/reconstruction/infra/ctrport_app0_check.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
