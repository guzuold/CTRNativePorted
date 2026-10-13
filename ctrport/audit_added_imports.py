# SPDX-License-Identifier: GPL-2.0-only
"""Audit added retail NIDs against pinned source registrations, without boot claims."""
import hashlib
import json
import re
from pathlib import Path
from inventory_imports import ROOT, public_names

if __name__ == '__main__':
    folder = ROOT / 'reports/reconstruction/infra'
    inventory = json.loads((folder / 'ctrport_imports_static.json').read_text(encoding='utf-8'))
    sources = sorted((ROOT / 'engine/shadPS4/src/core/libraries').rglob('*.cpp'))
    texts = [(p, p.read_bytes().decode('latin-1')) for p in sources]
    names = public_names()
    rows = []
    for identity in inventory['added_01_21']:
        matches = []
        for path, text in texts:
            pattern = r'LIB_FUNCTION\(\s*"' + re.escape(identity['nid']) + r'"\s*,\s*"' + re.escape(identity['library']) + r'"[^;]*;'
            for match in re.finditer(pattern, text):
                matches.append(dict(path=str(path.relative_to(ROOT)).replace('\\', '/'),
                                    line=text.count('\n', 0, match.start()) + 1,
                                    sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                                    license='GPL-2.0-or-later'))
        rows.append(dict(identity, known_name=names.get(identity['nid']),
                         source_registrations=matches,
                         source_status='registered_requires_behavior_review' if matches else 'no_exact_registration_found',
                         ctrport_status='missing'))
    assert len(rows) == 7 and sum(bool(r['source_registrations']) for r in rows) == 3
    report = dict(status='passed_static_source_audit', revision='e3ce810f3a653f43ac64ebab63023de281a4103a',
                  records=rows, scanned_cpp_files=len(sources),
                  observations=['WebBrowser Close and GetResult contain STUBBED logging; GetResult returns ORBIS_OK without result handling.',
                                'MsgDialog GetResult checks FINISHED and null argument then copies stored result.',
                                'The network-local strlcpy helper is not a libc NID registration.'],
                  limitations=['Registration is not runtime support or proof of the 1.21 boot failure.',
                               'No baseline execution or CTRPort service implementation performed.'])
    (folder / 'ctrport_added_import_source_audit.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps([(r['nid'], r['source_status']) for r in rows]))
