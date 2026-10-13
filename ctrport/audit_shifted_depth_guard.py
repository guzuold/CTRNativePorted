# SPDX-License-Identifier: GPL-2.0-only
"""Audit exact shifted depth owner transfer in one cold run."""
from pathlib import Path
import json,datetime,hashlib
cold=json.loads(Path('reports/reconstruction/infra/json2_compat_startup.json').read_text())
assert hashlib.sha256(Path('build/CTRPort-infra/ctrport.exe').read_bytes()).hexdigest()==cold['exe_sha256']
rows=[]
for l in Path('runtime/user/logs/submit_flip_census.jsonl').read_text(errors='replace').splitlines():
 if len(l)>4000:continue
 try:rows.append(json.loads(l))
 except ValueError:pass
control=json.loads(Path('runtime/user/logs/gpu_replace_extend_control.log').read_text().strip())
assert control['passed'] and control['negative_rejections']==5 and control['read_write_faults']==6
attempt=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_depth_retain_attempt' and x.get('word')==3622]
resident=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_depth_GPU_resident' and x.get('word')==3622]
eop=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_EOP_completion' and x.get('word')==3622]
draws=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_MRT_GPU_draw' and x.get('word')==2336]
terminal=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_ordered_route_handoff'][-1:]
passed=any(x.get('status')==0 for x in attempt) and any(x.get('status')==0 and x.get('generation',0)>=6 for x in resident) and any(x.get('status')==0 for x in eop) and not cold['original_access_faults']
r=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),SUP='SUP-029',scope='exact_shifted_whole_GPU_depth_owner_replaces_excluded_suffix_and_extends_guard',passed=passed,control=control,attempt=attempt,resident=resident,eop=eop,draw2336=draws,terminal=terminal,exe_sha256=cold['exe_sha256'],original_access_faults=cold['original_access_faults'],terminal_service_diagnostic=cold.get('service_diagnostics',[])[-1:],source_sha256={p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in ['ctrport/gpu_resident_memory.hpp','ctrport/depth_residency.hpp','ctrport/entry_boundary.hpp','ctrport/graphics_companion_client.hpp']},active_stubs=cold['active_stubs'],distinct_called_stubs={str(x['registry_index']):x['public_name'] for x in cold['called_stubs']},provenance='Project-authored infra exclusion ownership and padding authority. Real new whole GPU writer/retained provider token; no prof/new upstream code.',limits='Real-page synthetic ownership control is not an image pixel oracle. The shifted entry deliberately has NO prior/padding authority: new-layout padding can overlap old GPU logical writes. Future retile/publication must fail until real backing authority is established. Synthetic control prior bytes do not establish retail padding. Existing excluded pages remain NOACCESS; only additional owned RW pages become NOACCESS. No CPU publication, no fake shifted sampled view, no atomic unsynchronized guest claim. R4/R5/audio/navigation unverified.',R3=True,R4=False,R5=False,engine_replacements=0)
Path('reports/reconstruction/infra/shifted_depth_guard_transfer.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps({'passed':passed,'attempt':attempt,'resident':resident,'terminal':terminal,'diagnostic':r['terminal_service_diagnostic']}))
