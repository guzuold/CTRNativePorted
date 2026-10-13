# SPDX-License-Identifier: GPL-2.0-only
"""Synthetic overlay and retail inventory consistency controls, no game execution."""
import hashlib
import json
import struct
import tempfile
from pathlib import Path
from inventory_imports import ROOT, census, module_census


def synthetic_elf(marker):
    data = bytearray(512)
    data[:6] = b'\x7fELF\x02\x01'
    struct.pack_into('<Q', data, 32, 64)
    struct.pack_into('<HH', data, 54, 56, 1)
    struct.pack_into('<II6Q', data, 64, 1, 4, 0, 0, 0, 512, 512, 4096)
    data[400] = marker
    return bytes(data)


def main():
    workspace = ROOT / 'build/CTRPort-infra'
    workspace.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=workspace) as temporary:
        base, patch = Path(temporary) / 'base', Path(temporary) / 'patch'
        for directory in (base, patch):
            (directory / 'sce_module').mkdir(parents=True)
        (base / 'sce_module/a.prx').write_bytes(synthetic_elf(1))
        (base / 'sce_module/b.prx').write_bytes(synthetic_elf(2))
        (patch / 'sce_module/a.prx').write_bytes(synthetic_elf(3))
        before = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in Path(temporary).rglob('*.prx')}
        rows = module_census(base, patch)
        assert len(rows) == 2
        assert rows[0]['overlay_layer'] == 'patch'
        assert rows[0]['sha256'] == hashlib.sha256(synthetic_elf(3)).hexdigest()
        assert rows[1]['overlay_layer'] == 'base'
        assert before == {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in before}
        try:
            census(base / 'sce_module/a.prx', '01.00')
        except ValueError:
            pass
        else:
            raise AssertionError('unrecognized eboot accepted')
    report = json.loads((ROOT / 'reports/reconstruction/infra/ctrport_imports_static.json').read_text())
    versions = report['versions']
    assert [v['import_relocation_count'] for v in versions] == [1781, 1788]
    assert all(len(v['modules']) == 6 for v in versions)
    assert all(m['overlay_layer'] == 'base' for v in versions for m in v['modules'])
    assert [m['sha256'] for m in versions[0]['modules']] == [m['sha256'] for m in versions[1]['modules']]
    output = {'passed': True, 'synthetic_controls': ['patch precedence', 'base fallback', 'inputs unchanged', 'unknown eboot rejected'],
              'retail_modules_per_version': 6,
              'module_import_counts': {m['file']: m['import_relocation_count'] for m in versions[0]['modules']},
              'runtime_loaded': False, 'runtime_replacements': 0}
    (ROOT / 'reports/reconstruction/infra/ctrport_import_inventory_check.json').write_text(json.dumps(output, indent=2) + '\n')
    print(json.dumps(output))


if __name__ == '__main__':
    main()
