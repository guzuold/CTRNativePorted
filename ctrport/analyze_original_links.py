# SPDX-License-Identifier: GPL-2.0-only
"""Exact retail NID resolution candidates, without mapping or successful stubs."""
import argparse
import json
import sys
from pathlib import Path
from inventory_imports import ROOT, PINS
sys.path.insert(0, str(ROOT / 'tools/harness'))
from images import OriginalImage


def analyze(base, patch, version):
    executable = (patch if patch else base) / 'eboot.bin'
    eboot = OriginalImage(executable)
    if eboot.sha256 != PINS[version]:
        raise ValueError('selected eboot hash mismatch')
    paths = {p.name: p for p in (base / 'sce_module').glob('*.prx')}
    if patch:
        paths.update({p.name: p for p in (patch / 'sce_module').glob('*.prx')})
    images = [('eboot.bin', eboot)] + [('sce_module/' + n, OriginalImage(p)) for n, p in sorted(paths.items())]
    inventory = json.loads((ROOT / 'reports/reconstruction/infra/ctrport_imports_static.json').read_text())
    pinned = next(v for v in inventory['versions'] if v['version'] == version)
    pins = {m['file']: m['sha256'] for m in pinned['modules']}
    for name, image in images[1:]:
        if pins.get(name) != image.sha256:
            raise ValueError('module hash mismatch: ' + name)
    exports = {}
    for name, image in images:
        for symbol in image.exports:
            if not any(lo <= symbol['offset'] < hi for lo, hi in image.code_ranges):
                raise ValueError('original function export outside executable span')
            key = (symbol['nid'], symbol['library'], symbol['module'], symbol['type'])
            exports.setdefault(key, []).append({'image': name, 'image_sha256': image.sha256,
                                               'address': hex(symbol['offset'])})
    bindings, missing, ambiguous = [], [], []
    for name, image in images:
        for symbol in image.imports:
            key = (symbol['nid'], symbol['library'], symbol['module'], symbol['type'])
            matches = exports.get(key, [])
            record = {'source_image': name, 'source_sha256': image.sha256,
                      'slot': hex(symbol['slot']), 'nid': symbol['nid'],
                      'library': symbol['library'], 'module': symbol['module'],
                      'symbol_type': symbol['type'], 'relocation_type': symbol['relocation_type'],
                      'addend': symbol['addend'], 'runtime_bound': False}
            if len(matches) == 1 and symbol['type'] == 2:
                bindings.append(dict(record, target=matches[0]))
            elif matches:
                ambiguous.append(dict(record, candidates=matches, reason='ambiguous_or_nonfunction_export'))
            else:
                missing.append(record)
    return {'version': version, 'eboot_sha256': eboot.sha256,
            'exact_original_function_candidates': bindings, 'missing': missing,
            'ambiguous_or_unsupported': ambiguous,
            'counts': {'candidates': len(bindings), 'missing': len(missing), 'unsupported': len(ambiguous)},
            'runtime_bound': False, 'service_implementation_credit': 0}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', type=Path, required=True)
    parser.add_argument('--patch', type=Path, required=True)
    args = parser.parse_args()
    results = [analyze(args.base, None, '01.00'), analyze(args.base, args.patch, '01.21')]
    output = {'scope': 'static exact NID/library/module/type function resolution candidates; no runtime binding',
              'versions': results, 'R1': False, 'R2': False, 'runtime_replacements': 0}
    (ROOT / 'reports/reconstruction/infra/ctrport_original_link_candidates.json').write_text(json.dumps(output, indent=2) + '\n')
    print(json.dumps([v['counts'] for v in results]))


if __name__ == '__main__':
    main()
