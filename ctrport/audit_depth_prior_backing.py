# SPDX-License-Identifier: GPL-2.0-only
"""Audit original pre-guard backing retention; never grants GPU content equivalence."""
from pathlib import Path
import json,hashlib,datetime

def events(path):
 for l in Path(path).read_text(errors='replace').splitlines():
  if l.startswith('{') and len(l)<3000:
   try:yield json.loads(l)
   except ValueError:pass
r=json.loads(Path('reports/reconstruction/infra/json2_compat_startup.json').read_text())
mem=list(events('runtime/user/logs/prior_backing_memory_control.log'));depth=list(events('runtime/user/logs/prior_backing_depth_control.log'))
assert any(e.get('scope')=='GPU_resident_prior_backing_control' and e.get('passed') for e in mem)
assert any(e.get('scope')=='sample1_depth_GPU_residency_control' and e.get('passed') for e in depth)
census=list(events('runtime/user/logs/submit_flip_census.jsonl'))
layouts=[x for x in census if x.get('scope')=='compute_output_prior_depth_layout']
authority=[x for x in census if x.get('scope')=='compute_output_capture_authority']
assert layouts and authority and all(x['status']==0 and x['prior_backing_bytes']==x['guard_bytes'] and x['samples']==2 for x in layouts)
assert any(x['lease']==authority[0]['suffix_image'] and x['generation']==authority[0]['suffix_generation'] for x in layouts)
assert not r['original_access_faults']
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'owned_original_pre_guard_backing_retained_not_GPU_materialization','passed':True,'controls':mem+depth,'retail_layouts':layouts,'retail_capture_authority':authority,'exe_sha256':r['exe_sha256'],'source_sha256':{p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in ['ctrport/gpu_resident_memory.hpp','ctrport/depth_residency.hpp','ctrport/entry_boundary.hpp']},'active_stubs':r['active_stubs'],'called_stubs':{str(x['registry_index']):x['public_name'] for x in r['called_stubs']},'limits':'Original pre-guard bytes preserve unaffected padding authority only. GPU logical depth/stencil must come from real conversion/fence; no guard removal or guest publication. Provider pending. No milestone advancement.','R3':True,'R4':False,'R5':False,'engine_replacements':0}
Path('reports/reconstruction/infra/depth_prior_backing.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'passed':True,'layouts':len(layouts),'prior_bytes':[x['prior_backing_bytes'] for x in layouts]}))
