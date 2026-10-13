# SPDX-License-Identifier: GPL-2.0-only
"""Actual image descriptor updates joined to exact retail compiler identity."""
import datetime,hashlib,json
from pathlib import Path
source=Path('runtime/user/logs/submit_flip_census.jsonl')
events=[json.loads(line) for line in source.read_text().splitlines()]
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
stages={}
for e in startup['events']:
 if e.get('scope') in ('original_GnmSubmitFlip_first_graphics','original_GnmSubmitFlip_graphics_census'):
  resources=e.get('resources',e.get('compiler_resources'))
  if resources:stages[e['word'],e['vertex']]=resources
rows=[e for e in events if e.get('scope')=='original_GnmSubmitFlip_image_descriptor_update']
assert len(rows)==39 and all(r['write_status'] in (0,88) for r in rows)
assert next(r for r in rows if r['word']==4911)['write_status']==88
for r in rows:
 assert r['layout_status']==0 and r['live_image_tokens']==53 and not r['pipeline_created'] and not r['gpu_execution']
 if r['write_status']:assert not r['writes'] and r['prepare_status']==88;continue
 assert r['prepare_status']==0 and r['image_binding_count']==len(r['writes'])
 assert len({w['binding'] for w in r['writes']})==len(r['writes'])
 for vertex in (False,True):
  expected=[b['sharp_index'] for b in stages[r['word'],vertex]['bindings'] if b['kind']==1]
  actual=[w['sharp_index'] for w in r['writes'] if w['vertex']==vertex]
  assert actual==expected
 assert all(w['live_token'] for w in r['writes'])
count=sum(r['image_binding_count'] for r in rows if r['write_status']==0)
ready=sum(r['write_status']==0 for r in rows)
assert ready>0
exe=Path(startup['exe_path']);assert hashlib.sha256(exe.read_bytes()).hexdigest()==startup['exe_sha256']
manifest=json.loads(exe.with_name(exe.name+'.build.json').read_text(encoding='utf-8-sig'))
assert manifest['executable_sha256']==startup['exe_sha256']
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'actual selected-session image descriptor updates; no pipeline/draw','exe_sha256':startup['exe_sha256'],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'build_input_manifest':manifest,'rows':rows,'updated_layouts':ready,'unsupported_layouts':len(rows)-ready,'image_descriptors_written':count,'peak_live_images':53,'R3':False,'engine_replacements':0,'GPU_execution':False,'active_stubs':startup['active_stubs'],'called_stubs':startup['called_stubs'],'payload_persisted':False}
Path('reports/reconstruction/infra/image_descriptor_updates.json').write_text(json.dumps(report,indent=2)+'\n')
print(f'{ready} layouts image-updated:{count}descriptors,53actualimages coexist;4911 remains88')
