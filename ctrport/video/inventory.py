# SPDX-License-Identifier: GPL-2.0-only
"""Rebuild the retail graphics import/source join; does not certify services."""
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'engine/shadPS4'
FILES = ['src/core/libraries/gnmdriver/gnmdriver.cpp',
         'src/core/libraries/videoout/video_out.cpp']


def generate():
    manifest = ROOT / 'reports/reconstruction/infra/ctrport_imports_complete.json'
    imports = json.loads(manifest.read_text(encoding='utf-8-sig'))
    registrations = {}
    sources = []
    for file in FILES:
        raw = (SOURCE / file).read_bytes()
        code = raw.decode('utf-8-sig')
        sources.append({'file': file, 'sha256': hashlib.sha256(raw).hexdigest(),
                        'license': 'GPL-2.0-or-later',
                        'direct_includes': re.findall(r'^#include [<"]([^>"\n]+)', code, re.M)})
        for match in re.finditer(r'LIB_FUNCTION\("([^"]+)",\s*"([^"]+)",\s*\d+,\s*"([^"]+)",\s*(\w+)\)', code):
            nid, library, module, function = match.groups()
            definitions = list(re.finditer(r'\b' + re.escape(function) + r'\s*\([^;{}]*\)\s*\{', code))
            if len(definitions) != 1:
                raise ValueError((function, len(definitions)))
            start = definitions[0].end()
            # Balanced braces suffice for these source bodies; flags are lexical evidence only.
            depth, end = 1, start
            while depth:
                depth += (code[end] == '{') - (code[end] == '}')
                end += 1
            body = code[start:end - 1]
            registrations[(library, nid)] = {
                'source_file': file, 'function': function,
                'definition_line': code.count('\n', 0, definitions[0].start()) + 1,
                'registration_line': code.count('\n', 0, match.start()) + 1,
                'upstream_body_flags': [flag for flag in ('STUBBED', 'UNREACHABLE', 'UNIMPLEMENTED') if flag in body],
                'runtime_status': 'missing_not_bound'}
    versions = []
    for version in imports['versions']:
        selected = {}
        for item in version['records']:
            if item['library'] not in ('libSceGnmDriver', 'libSceGnmDriverResourceRegistration', 'libSceVideoOut'):
                continue
            key = (item['library'], item['nid'])
            row = {'library': key[0], 'nid': key[1], 'retail_name': item.get('known_name'),
                   **registrations.get(key, {'source_file': None, 'function': None,
                                            'runtime_status': 'missing_not_bound'})}
            selected[key] = row
        rows = sorted(selected.values(), key=lambda row: (row['library'], row['nid']))
        versions.append({'version': version['version'], 'eboot_sha256': version['eboot_sha256'],
                         'counts': {lib: sum(row['library'] == lib for row in rows)
                                    for lib in ('libSceGnmDriver', 'libSceGnmDriverResourceRegistration', 'libSceVideoOut')},
                         'source_matches': sum(row['source_file'] is not None for row in rows),
                         'records': rows})
    assert all(row['nid'] and row['library'] for v in versions for row in v['records'])
    return {'schema': 1, 'upstream_revision': 'e3ce810f3a653f43ac64ebab63023de281a4103a',
            'upstream_label': 'shadPS4 v0.18.0',
            'method': 'Exact library/NID registration join and unique source definition; lexical body flags are not semantic validation. No upstream stub is approved for runtime binding.',
            'input_sha256': hashlib.sha256(manifest.read_bytes()).hexdigest(),
            'sources': sources, 'versions': versions}


if __name__ == '__main__':
    output = ROOT / 'ctrport/video/import_inventory.json'
    output.write_text(json.dumps(generate(), indent=2) + '\n', encoding='utf-8')
