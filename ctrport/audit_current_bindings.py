# SPDX-License-Identifier: GPL-2.0-only
"""Audit current source binding candidates against every pinned retail import slot.
This is a static census, never an implementation or execution coverage claim.
"""
import hashlib
import json
import re
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPORTS = ROOT / 'reports/reconstruction/infra'

def audit():
    source = ROOT / 'ctrport/entry_boundary.hpp'
    text = source.read_text()
    pattern = r'services.push_back\(\{"([^"]+)","([^"]+)","([^"]+)",reinterpret_cast<const void\*>\(&([A-Za-z0-9_]+)\)\}\);'
    matches = list(re.finditer(pattern, text))
    assert len(matches) == text.count('services.push_back('), 'Unparsed binding declaration'
    candidates = {}
    for match in matches:
        nid, library, module, target = match.groups()
        key = (nid, library, module)
        assert key not in candidates, ('Duplicate static binding', key)
        candidates[key] = dict(nid=nid, library=library, module=module,
                               target=target, line=text.count('\n', 0, match.start())+1,
                               status='conditional_source_candidate_not_support_claim')
    assert candidates[('4wSze92BhLI','libkernel','libkernel')]['target'] == 'ctr_sysv_console_write'
    inventory = REPORTS / 'ctrport_imports_complete.json'
    historical = json.loads(inventory.read_text())
    versions = []
    for version in historical['versions']:
        records = []
        for row in version['records']:
            key = tuple(row[k] for k in ('nid','library','module'))
            binding = candidates.get(key) if row['symbol_type'] == 2 else None
            records.append(dict(source_sha256=row['source_sha256'], slot=row['slot'],
                                nid=row['nid'], library=row['library'], module=row['module'],
                                known_name=row['known_name'], symbol_type=row['symbol_type'],
                                original_status=row['status'], current_source_candidate=binding,
                                current_status='conditional_native_candidate' if binding else
                                'original_bound_historical' if row['status']=='original_bound_pointer_verified'
                                else 'missing_in_this_static_function_census',
                                static_callers_reference=dict(path=inventory.relative_to(ROOT).as_posix(),
                                source_sha256=row['source_sha256'], slot=row['slot'])))
        versions.append(dict(version=version['version'],eboot_sha256=version['eboot_sha256'],
                             records=records, counts=dict(Counter(r['current_status'] for r in records))))
    return dict(scope='current_static_function_binding_census', candidates=list(candidates.values()),
                versions=versions, inputs=[dict(path=p.relative_to(ROOT).as_posix(),
                sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in (source,inventory)],
                limitations=['Conditions are not evaluated; a source candidate is not necessarily active.',
                'Data bindings and dynamically activated original modules are not reclassified.',
                'Historical original binding status is retained separately, not reverified.',
                'No service is promoted to fully implemented and no runtime coverage is inferred.'])

if __name__ == '__main__':
    report=audit()
    (REPORTS/'ctrport_current_binding_census.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'candidates':len(report['candidates']),
                      'versions':[{'version':v['version'],'counts':v['counts']} for v in report['versions']]}))
