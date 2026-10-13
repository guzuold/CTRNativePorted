# SPDX-License-Identifier: GPL-2.0-only
"""Verify owned image extents from companion, without recalculating tiling."""
import collections
import hashlib
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
base=ROOT/'reports/reconstruction/infra'
source=base/'descriptor_capture_census.json'
raw=source.read_bytes();r=json.loads(raw)
exe=ROOT/json.loads((base/'shareplay_pending_startup.json').read_text())['exe_path']
assert exe.resolve().parent==(ROOT/'build/CTRPort-infra').resolve()
assert hashlib.sha256(exe.read_bytes()).hexdigest()==r['exe_sha256']
images=[d for d in r['descriptor_observations'] if d['kind']==1]
assert len(images)==67
accepted=[];missing=[]
for d in images:
 if d['image_extent_status']:
  assert d['image_extent_status']==88 and not d['image_payload_owned']
  missing.append({k:d[k] for k in ('word','vertex','sharp_index','image_status','image_extent_status')})
  if not d['image_status']:missing[-1]['parameters']=d['image_parameters']
  continue
 p=d['image_parameters'];x=d['image_extent'];b=d['image_copy']
 assert d['image_status']==0 and d['image_payload_owned']
 assert p['type'] in (8,9,14) and p['tile_mode'] in (1,8,13,14) and p['data_format'] in (3,4,5,10)
 assert p['layers']==p['levels']==1 and p['samples'] in (1,2) and not any(p[k] for k in ('pow2pad','alt_tile','compression','base_level','base_array','last_array'))
 assert x['address']==p['guest_address'] and x['pitch']==p['pitch'] and x['padded_height']>=p['height']
 assert 0<x['bytes']<=32*1024*1024 and b['copied_bytes']==x['bytes']
 assert all(b[k]==0 for k in ('requirement_status','lease_status','capture_status','release_status','lease_end_status'))
 accepted.append({k:d[k] for k in ('word','vertex','sharp_index','image_extent','image_copy')})
assert len(accepted)==66 and len(missing)==1
out={'scope':'retail_owned_image_extent_capture','eboot_sha256':r['eboot_sha256'],'exe_sha256':r['exe_sha256'],'source_sha256':hashlib.sha256(raw).hexdigest(),'accepted_count':len(accepted),'copied_bytes':sum(d['image_extent']['bytes'] for d in accepted),'unsupported_count':len(missing),'accepted':accepted,'missing':missing,'per_image_max_bytes':32*1024*1024,'per_stage_image_budget':64*1024*1024,'whole_submission_budget':512*1024*1024,'R3':False,'engine_replacements':0,'active_stubs':r['active_stubs'],'called_stubs':r['called_stubs'],'gpu_admission':False,'source_writer_coherence':False,'payload_persisted':False,'limitation':'Exact owned CPU copies of companion-admitted retail image spans only. No manual width*height byte sizing. Unsupported image variants remain88; no detiling/upload, GPU image creation/binding, writer coherence, draw/dispatch or flip. Copies from separate draws are retained separately because no cross-draw coherence authority exists.','repeat':'python -B ctrport/audit_image_extent_capture.py'}
(base/'image_extent_capture_census.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({'accepted':len(accepted),'copied_bytes':out['copied_bytes'],'unsupported':len(missing),'R3':False}))
