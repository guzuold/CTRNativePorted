# SPDX-License-Identifier: GPL-2.0-only
"""Exact original DrawIndex2 CPU inputs, not GPU execution or generated indices."""
import hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
base=root/'reports/reconstruction/infra'
source=base/'shareplay_pending_startup.json';raw=source.read_bytes();r=json.loads(raw)
assert hashlib.sha256((root/r['exe_path']).read_bytes()).hexdigest()==r['exe_sha256']
events=[e for e in r['events'] if e.get('scope')=='original_GnmSubmitFlip_index_capture']
ordered=next(e for e in r['events'] if e.get('scope')=='original_GnmSubmitFlip_ordered')
assert [e['word'] for e in events]==[p[0] for p in ordered['packets'] if p[2]==0x27]
assert len(events)==2
for e in events:
 assert e['type_observed'] and e['type'] in (0,1)
 assert all(e[k]==0 for k in ('requirement_status','capture_status','lease_status','release_status','lease_end_status','retained_status'))
 assert e['bytes']==e['copied_bytes']>0 and e['count']>0 and not e['gpu_execution']
joins=[e for e in r['events'] if e.get('scope')=='original_GnmSubmitFlip_index_layout_join']
assert [x['word'] for x in joins]==[x['word'] for x in events]
for x,e in zip(joins,events):
 assert x['stride']==2 and x['bytes']==e['bytes'] and x['index_token'] and x['layout_token'] and x['unique_join'] and not x['GPU_execution']
out={'scope':'retail_exact_owned_index_inputs','eboot_sha256':'ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1',
 'exe_sha256':r['exe_sha256'],'source_sha256':hashlib.sha256(raw).hexdigest(),'events':events,'layout_joins':joins,
 'total_bytes':sum(e['bytes'] for e in events),'active_stubs':r['active_stubs'],'called_stubs':r['called_stubs'],
 'R3':False,'engine_replacements':0,'payload_persisted':False,
 'public_source':{'revision':'e3ce810f3a653f43ac64ebab63023de281a4103a','license':'GPL-2.0-or-later','files':['src/video_core/amdgpu/pm4_cmds.h','src/video_core/amdgpu/regs_vertex.h','src/video_core/buffer_cache/buffer_cache.cpp']},
 'limitation':'Exact guest index bytes retained in CPU batch only. Standalone Vulkan index initialization is separately evidenced in buffer_initialization_lifecycle.json. No generated indices, byte-swap modes, primitive conversion, index binding, writer coherence or draw execution. SharePlay caller parked without return.',
 'repeat':'python -B ctrport/audit_index_capture.py'}
(base/'index_capture_census.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({'indices':len(events),'bytes':out['total_bytes'],'R3':False}))
