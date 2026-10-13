# SPDX-License-Identifier: GPL-2.0-only
"""Actual sampler descriptor updates joined to exact retail compiler identity."""
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
rows=[e for e in events if e.get('scope')=='original_GnmSubmitFlip_sampler_descriptor_update']
assert len(rows)==39 and [(r['word'],r['write_status']) for r in rows if r['write_status']]==[(4911,88)]
for r in rows:
 assert r['layout_status']==0 and r['live_sampler_tokens']==64 and not r['images_written'] and not r['gpu_execution']
 if r['write_status']:assert not r['writes'] and r['prepare_status']==88;continue
 assert r['prepare_status']==0 and r['sampler_binding_count']==len(r['writes'])
 assert len({w['binding'] for w in r['writes']})==len(r['writes'])
 for vertex in (False,True):
  expected=[b['sharp_index'] for b in stages[r['word'],vertex]['bindings'] if b['kind']==2]
  actual=[w['sharp_index'] for w in r['writes'] if w['vertex']==vertex]
  assert actual==expected
 assert all(w['live_token'] for w in r['writes'])
assert sum(r['sampler_binding_count'] for r in rows)==54
exe=Path(startup['exe_path']);assert hashlib.sha256(exe.read_bytes()).hexdigest()==startup['exe_sha256']
manifest=json.loads(exe.with_name(exe.name+'.build.json').read_text(encoding='utf-8-sig'))
assert manifest['executable_sha256']==startup['exe_sha256']
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'actual selected-session sampler descriptor updates; no images/pipeline/draw','exe_sha256':startup['exe_sha256'],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'build_input_manifest':manifest,'rows':rows,'updated_layouts':38,'sampler_descriptors_written':54,'peak_live_samplers':64,'R3':False,'engine_replacements':0,'GPU_execution':False,'active_stubs':startup['active_stubs'],'called_stubs':startup['called_stubs'],'payload_persisted':False}
Path('reports/reconstruction/infra/sampler_descriptor_updates.json').write_text(json.dumps(report,indent=2)+'\n')
print('38 layouts sampler-updated:54descriptors,64actualsamplers coexist;4911 remains88')
