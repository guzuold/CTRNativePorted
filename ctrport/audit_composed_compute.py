# SPDX-License-Identifier: GPL-2.0-only
from pathlib import Path
import json,hashlib,datetime
cold=json.loads(Path('reports/reconstruction/infra/json2_compat_startup.json').read_text())
rows=[]
for line in Path('runtime/user/logs/submit_flip_census.jsonl').read_text(errors='replace').splitlines():
 if line.startswith('{') and len(line)<3000:
  try:rows.append(json.loads(line))
  except ValueError:pass
scopes=['compute_output_depth_GPU_snapshot','compute_output_composed_snapshot','compute_output_GPU_result_before_publication','original_GnmSubmitFlip_compute_GPU_dispatch']
events={k:[x for x in rows if x.get('scope')==k and x.get('word')==755] for k in scopes}
control=json.loads(Path('runtime/user/logs/depth_prefix_snapshot_control.log').read_text().strip())
assert control['passed'] and control['rejections']==6
assert hashlib.sha256(Path('build/CTRPort-infra/ctrport.exe').read_bytes()).hexdigest()==cold['exe_sha256']
composed=events[scopes[1]];gpu=events[scopes[2]]
passed=bool(composed and gpu and all(x['status']==0 and x['bytes']==8355840 and x['CPU_prefix_bytes']==2064384 for x in composed) and all(x['dispatch_status']==0 and x['download_status']==0 and x['bytes']==8355840 and not x['command_completed'] for x in gpu) and not cold['original_access_faults'])
r=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),SUP='SUP-029',scope='owned_host_composed_initial_image_and_original_compute_before_guest_publication',provenance='Original infra composition/orchestration, existing project-authored MS2 extraction shader and previously pinned data provider; no new upstream/prof-derived code.',passed=passed,control=control,retail_events=events,exe_sha256=cold['exe_sha256'],source_sha256={p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in ['ctrport/depth_gpu_snapshot.hpp','ctrport/entry_boundary.hpp']},active_stubs=cold['active_stubs'],distinct_called_stubs={str(x['registry_index']):x['public_name'] for x in cold['called_stubs']},original_access_faults=cold['original_access_faults'],terminal_boundary=cold['terminal_boundary'],terminal_service_diagnostic=cold.get('service_diagnostics',[])[-1:],limits='Host snapshot and GPU dispatch/download only; no independent retail pixel oracle, guest publication, exclusion lease split, old full-depth alias invalidation, EOS or completed command. Prefix repeated read rejects observed mutation but is not an atomic guest snapshot. No new stub or engine replacement.',R3=True,R4=False,R5=False,engine_replacements=0)
Path('reports/reconstruction/infra/compute755_composed_GPU_result.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps({'passed':passed,'events':events}))
