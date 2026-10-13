# SPDX-License-Identifier: GPL-2.0-only
"""Check retail buffer token inputs; no Vulkan descriptor writes claimed."""
import datetime,hashlib,json
from pathlib import Path
source=Path('runtime/user/logs/submit_flip_census.jsonl')
events=[json.loads(x) for x in source.read_text().splitlines()]
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
stages={}
for e in startup['events']:
 if e.get('scope') in ('original_GnmSubmitFlip_first_graphics','original_GnmSubmitFlip_graphics_census'):
  owned=e.get('owned_stage',e.get('owned_compiled_stage'))
  resources=e.get('resources',e.get('compiler_resources'))
  if owned and resources: stages[e['word'],e['vertex']]=(owned,resources)
rows=[e for e in events if e.get('scope')=='original_GnmSubmitFlip_buffer_write_inputs']
expected=[e['word'] for e in events if e.get('scope')=='original_GnmSubmitFlip_graphics_pair' and e['status']==0]
assert len(rows)==39 and [r['word'] for r in rows]==expected
assert [(r['word'],r['status']) for r in rows if r['status']] == [(4911,88)]
for r in rows:
 assert not r['descriptor_writes'] and not r['gpu_execution']
 if r['status']: assert not r['writes'];continue
 assert len({w['binding'] for w in r['writes']})==len(r['writes'])
 assert len({w['buffer_slot'] for w in r['writes']})==len(r['writes'])
 before=0
 for vertex in (False,True):
  owned,res=stages[r['word'],vertex]
  bindings=[b for b in res['bindings'] if b['kind']==0]
  writes=[w for w in r['writes'] if w['vertex']==vertex]
  assert len(writes)==len(bindings)
  for ordinal,(w,b) in enumerate(zip(writes,bindings)):
   assert w['buffer_slot']==before+ordinal
   assert (w['sharp_index'],w['buffer_type'])==(b['sharp_index'],b['buffer_type'])
   if w['null_clip_planes']:
    assert w['buffer_type']==6 and not w['live_token'] and w['bytes']==2**64-1
    clip=next(c for c in owned['clip_planes'] if c['sharp_index']==w['sharp_index'])
    assert clip['status']==clip['enabled_mask']==clip['bytes']==0
   else:
    assert w['live_token'] and w['bytes']>0
    collection=owned['buffers'] if w['buffer_type']==0 else owned['flatbufs']
    captured=next(c for c in collection if c['sharp_index']==w['sharp_index'])
    assert captured['bytes']==w['bytes']
  before+=len(bindings)
writes=[w for r in rows for w in r['writes']]
assert len(writes)==92 and sum(w['null_clip_planes'] for w in writes)==38
exe=Path(startup['exe_path']);assert hashlib.sha256(exe.read_bytes()).hexdigest()==startup['exe_sha256']
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'retail buffer-token preparation only; no descriptor writes','exe_sha256':startup['exe_sha256'],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'pairs':39,'ready_pairs':38,'live_buffer_references':54,'explicit_null_clip_planes':38,'rows':rows,'active_stubs':startup['active_stubs'],'called_stubs':startup['called_stubs'],'R3':False,'engine_replacements':0,'descriptor_writes':False,'GPU_execution':False}
Path('reports/reconstruction/infra/buffer_write_inputs.json').write_text(json.dumps(report,indent=2)+'\n')
print('38 ready pairs:54 live buffer references+38 explicit null ClipPlanes;4911 remains88')
