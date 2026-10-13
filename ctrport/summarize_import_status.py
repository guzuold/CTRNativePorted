# SPDX-License-Identifier: GPL-2.0-only
"""Join verified native slot evidence with the public-name retail census."""
import hashlib
import json
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPORTS = ROOT / 'reports/reconstruction/infra'


def summarize():
    service_path=REPORTS/'ctrport_services.json'
    service_inventory=json.loads(service_path.read_text()) if service_path.exists() else None
    inputs = ['ctrport_imports_static.json', 'ctrport_native_import_status.json']
    caller_path = REPORTS / 'ctrport_static_callers.json'
    callers = json.loads(caller_path.read_text()) if caller_path.exists() else None
    census, native = [json.loads((REPORTS / name).read_text(encoding='utf-8'))
                      for name in inputs]
    assert native['status'] == 'passed'
    versions = []
    for static, measured in zip(census['versions'], native['versions']):
        assert static['version'] == measured['version']
        images = [static] + static['modules']
        identities = {}
        for image in images:
            for row in image['imports']:
                key = (image['sha256'], row['slot'])
                assert key not in identities
                identities[key] = row
        records = []
        for row in measured['records']:
            original = identities.pop((row['source_sha256'], row['slot']))
            for field in ('nid', 'library', 'module', 'relocation_type'):
                assert row[field] == original[field]
            assert row['symbol_type'] == original['type']
            records.append(dict(row, known_name=original['known_name'],
                                implementation=None, reused_service_source=None,
                                thunk_addresses=original['thunk_addresses'],
                                callers_status='not_observed_for_this_slot'))
        if callers:
            sites = next(v for v in callers['versions'] if v['eboot_sha256']==static['eboot_sha256'])
            normalize = lambda slot: int(slot,16) if isinstance(slot,str) else slot
            grouped = {}
            for image in [dict(sites,image_sha256=static['eboot_sha256'])]+sites.get('modules',[]):
                for site in image['sites']:
                    key=(image['image_sha256'],normalize(site['slot']))
                    grouped.setdefault(key, []).append({'address':site['caller'],'kind':site['kind'],'function':site['containing_function']})
            missing_images={m['image_sha256']:m['diagnostic'] for m in sites.get('modules',[]) if m.get('status')=='missing_supported_FDE_inventory'}
            for record in records:
                record['static_callers']=grouped.get((record['source_sha256'],normalize(record['slot'])),[])
                record['callers_status']='missing_supported_FDE_inventory' if record['source_sha256'] in missing_images else 'static_FDE_boundary_sites_execution_unobserved'
                if record['source_sha256'] in missing_images:record['callers_missing_reason']=missing_images[record['source_sha256']]
        assert not identities
        configured_services=[]
        if service_inventory and service_inventory['eboot_sha256']==static['eboot_sha256']:
            configured_services=service_inventory['services']
        for record in records:
            record['configured_status']=record['status']
            for service in configured_services:
                if all(record[field]==service[field] for field in ('nid','library','module','symbol_type')):
                    record['configured_status']='implemented'
                    record['implementation']=service
                    record['configuration']=service_inventory['configuration']
                    record['measured_status_scope']='historical original-only native binding census'
        versions.append(dict(version=static['version'],
                             eboot_sha256=static['eboot_sha256'], records=records,
                             counts=dict(Counter(r['status'] for r in records)),
                             implemented_system_services=len(configured_services),
                             configured_counts=dict(Counter(r['configured_status'] for r in records))))
    return dict(schema=1, status='passed_complete_inventory_join', versions=versions,
                added_01_21=census['added_01_21'], removed_01_21=census['removed_01_21'],
                public_names_provenance=census['public_names_provenance'],
                static_callers_evidence={'path':'reports/reconstruction/infra/ctrport_static_callers.json','sha256':hashlib.sha256(caller_path.read_bytes()).hexdigest()} if callers else None,
                evidence=[dict(path='reports/reconstruction/infra/' + name,
                               sha256=hashlib.sha256((REPORTS / name).read_bytes()).hexdigest())
                          for name in inputs],
                limitations=['Original binding is not a Windows service implementation.',
                             'Thunks are not callers. Unobserved slot callers remain unknown.',
                             'TLS unresolved; main not reached. Static register-indirect caller recovery incomplete; no execution coverage claimed.'],
                R1=bool(callers and all(len(v.get('modules',[]))==6 and all(m.get('status')!='missing_supported_FDE_inventory' for m in v['modules']) for v in callers['versions'])), R2=False, runtime_replacements=0)


if __name__ == '__main__':
    report = summarize()
    (REPORTS / 'ctrport_imports_complete.json').write_text(
        json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps([dict(version=v['version'], count=len(v['records']), counts=v['counts'])
                      for v in report['versions']]))
