# SPDX-License-Identifier: GPL-2.0-only
"""Read-only retail import census. No loading, execution or service credit."""
import argparse
import hashlib
import json
import sys
import re
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/harness'))
from images import OriginalImage

PINS = {
    '01.00': 'ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1',
    '01.21': '39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5',
}

NAME_TABLE = ROOT / 'engine/shadPS4/src/core/aerolib/aerolib.inl'


def public_names():
    names = {}
    for nid, name in re.findall(r'STUB\(\s*"([^"\n]+)"\s*,\s*([^\s,)]+)\s*\)', NAME_TABLE.read_text(encoding='utf-8')):
        names.setdefault(nid, set()).add(name)
    return {nid: next(iter(values)) for nid, values in names.items() if len(values) == 1}


def image_census(path):
    image = OriginalImage(path)
    names = public_names()
    thunks = image.thunks()
    rows = []
    for item in image.imports:
        row = dict(item)
        row.update(known_name=names.get(item['nid']), status='missing', implementation=None,
                   thunk_addresses=[hex(a) for a, s in thunks.items()
                                    if s['slot'] == item['slot']],
                   callers=None,
                   callers_status='not_analyzed_thunks_are_not_callers')
        rows.append(row)
    return {'sha256': image.sha256, 'imports': rows,
            'import_relocation_count': len(rows),
            'unique_import_count': len({(x['nid'], x['library'], x['module']) for x in rows}),
            'by_library': dict(Counter(x['library'] for x in rows)),
            'unsupported_relocations': image.unsupported_relocations,
            'relative_relocation_count': len(image.relative_relocations),
            'runtime_loaded': False, 'runtime_implemented_imports': 0}


def census(path, version):
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != PINS[version]:
        raise ValueError('Retail eboot hash mismatch: ' + version)
    return dict(image_census(path), eboot_sha256=digest, version=version)


def module_census(base, patch=None):
    paths = {p.name: (p, 'base') for p in (base / 'sce_module').glob('*.prx')}
    if patch:
        paths.update({p.name: (p, 'patch') for p in (patch / 'sce_module').glob('*.prx')})
    return [dict(image_census(p), file='sce_module/' + name, overlay_layer=layer)
            for name, (p, layer) in sorted(paths.items())]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base-eboot', type=Path, required=True)
    parser.add_argument('--patch-eboot', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    destination = args.output.resolve()
    destination.relative_to(ROOT)
    results = [census(args.base_eboot, '01.00'), census(args.patch_eboot, '01.21')]
    results[0]['modules'] = module_census(args.base_eboot.parent)
    results[1]['modules'] = module_census(args.base_eboot.parent, args.patch_eboot.parent)
    sets = [{(x['nid'], x['library'], x['module']) for x in v['imports']} for v in results]
    def serialize(items):
        return [dict(nid=n, library=l, module=m) for n, l, m in sorted(items, key=repr)]
    report = {'schema': 2, 'scope': 'retail_static_eboot_and_overlay_modules_import_relocations',
              'versions': results, 'added_01_21': serialize(sets[1] - sets[0]),
              'removed_01_21': serialize(sets[0] - sets[1]),
              'limitations': ['callers not analyzed',
                              'unknown or ambiguous public names remain null', 'no runtime service implementation credit'],
              'public_names_provenance': {'file': str(NAME_TABLE.relative_to(ROOT)),
                                          'sha256': hashlib.sha256(NAME_TABLE.read_bytes()).hexdigest(),
                                          'license': 'GPL-2.0-or-later', 'scope': 'public NID names only, no service code reused'},
              'parser_provenance': {'file': 'tools/harness/images.py',
                                    'sha256': hashlib.sha256((ROOT / 'tools/harness/images.py').read_bytes()).hexdigest(),
                                    'origin': 'project infra oracle; reused by import, no external file copied'}}
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'counts': [v['import_relocation_count'] for v in results],
                      'added': len(report['added_01_21']), 'removed': len(report['removed_01_21'])}))


if __name__ == '__main__':
    main()
