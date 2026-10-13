# SPDX-License-Identifier: GPL-2.0-only
"""Audit one cold submit-local depth authority fix; no milestone promotion."""
from pathlib import Path
import datetime,hashlib,json
cold=json.loads(Path('reports/reconstruction/infra/json2_compat_startup.json').read_text())
assert hashlib.sha256(Path('build/CTRPort-infra/ctrport.exe').read_bytes()).hexdigest()==cold['exe_sha256']
rows=[]
for line in Path('runtime/user/logs/submit_flip_census.jsonl').read_text(errors='replace').splitlines():
 if len(line)>4000:continue
 try:rows.append(json.loads(line))
 except ValueError:pass
control=json.loads(Path('runtime/user/logs/submission_depth_authority_control.log').read_text().strip())
assert control['passed'] and control['expired_owner_rejected'] and control['capture_failure_preserved']
scopes=['original_GnmSubmitFlip_image_descriptor_update','original_GnmSubmitFlip_draw_resource_inputs','original_GnmSubmitFlip_submission_depth_authority','original_GnmSubmitFlip_graphics_depth_sampled_active','original_GnmSubmitFlip_MRT_GPU_draw']
events=[x for x in rows if x.get('word')==2336 and x.get('scope') in scopes]
complete=any(x.get('scope')==scopes[1] and x.get('status')==0 and x.get('descriptor_complete') for x in events)
authorized=any(x.get('scope')==scopes[2] and x.get('owner_status')==0 and x.get('compute_disjoint') for x in events)
executed=any(x.get('scope')==scopes[4] and x.get('status')==0 and x.get('invoked') and x.get('GPU_execution') for x in events)
terminal=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_ordered_route_handoff'][-1:]
report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),SUP='SUP-029',scope='submit_local_exact_depth_alias_authority',passed=complete and authorized and executed and not cold['original_access_faults'],control=control,events=events,terminal=terminal,exe_sha256=cold['exe_sha256'],original_access_faults=cold['original_access_faults'],terminal_service_diagnostic=cold.get('service_diagnostics',[])[-1:],source_sha256={p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in ['ctrport/entry_boundary.hpp','ctrport/graphics_companion_client.hpp']},active_stubs=cold['active_stubs'],distinct_called_stubs={str(x['registry_index']):x['public_name'] for x in cold['called_stubs']},provenance='Project-authored infra ownership admission. Original retail descriptor/whole depth identity validated against same-submit original680 writer; genuine provider alias and descriptor write required. No prof or new upstream code.',limits='Control synthetic lifetime/metadata, not GPU pixels. Weak submit lifetime cannot establish cross-submit authority. Original CPU capture failure remains recorded; no copied old suffix, CPU upload, whole old backing coherence, atomic guest publication, R4/R5 or reconstruction credit.',R3=True,R4=False,R5=False,engine_replacements=0)
Path('reports/reconstruction/infra/submission_depth_authority.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'passed':report['passed'],'events':events,'terminal':terminal,'diagnostic':report['terminal_service_diagnostic']}))
