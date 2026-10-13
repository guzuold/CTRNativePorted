# SPDX-License-Identifier: GPL-2.0-only
"""Actual selected-session storage descriptor updates, not a shader draw."""
import datetime,hashlib,json
from pathlib import Path
source=Path('runtime/user/logs/submit_flip_census.jsonl')
events=[json.loads(line) for line in source.read_text().splitlines()]
prepared={e['word']:e for e in events if e.get('scope')=='original_GnmSubmitFlip_buffer_write_inputs'}
rows=[e for e in events if e.get('scope')=='original_GnmSubmitFlip_buffer_descriptor_update']
layouts=[e for e in events if e.get('scope')=='original_GnmSubmitFlip_paired_layout']
assert len(rows)==39 and [e['word'] for e in rows]==[e['word'] for e in layouts]
assert [(r['word'],r['write_status']) for r in rows if r['write_status']] == [(4911,88)]
for index,r in enumerate(rows):
 p=prepared[r['word']]
 assert r['prepare_status']==p['status'] and r['layout_status']==0
 assert r['live_buffer_tokens']==58 and r['live_layout_tokens']==index+1
 assert not r['images_samplers_written'] and not r['gpu_execution']
 if r['write_status']: assert r['buffer_binding_count']==0
 else: assert r['buffer_binding_count']==len(p['writes'])
assert sum(r['buffer_binding_count'] for r in rows)==92
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
exe=Path(startup['exe_path']);assert hashlib.sha256(exe.read_bytes()).hexdigest()==startup['exe_sha256']
manifest=json.loads(exe.with_name(exe.name+'.build.json').read_text(encoding='utf-8-sig'))
assert manifest['executable_sha256']==startup['exe_sha256']
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'actual VkUpdateDescriptorSets storage-only selected-session updates; diagnostic parked SharePlay path','exe_sha256':startup['exe_sha256'],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'build_input_manifest':manifest,'rows':rows,'updated_layouts':38,'storage_descriptors_written':92,'live_buffer_references':54,'explicit_null_clip_planes':38,'peak_live_layouts':39,'peak_live_buffers':58,'images_samplers_written':False,'R3':False,'engine_replacements':0,'GPU_execution':False,'active_stubs':startup['active_stubs'],'called_stubs':startup['called_stubs'],'payload_persisted':False}
Path('reports/reconstruction/infra/buffer_descriptor_updates.json').write_text(json.dumps(report,indent=2)+'\n')
print('38 layouts storage-updated:92 writes,54live+38explicitnull;39layouts/58buffers coexist')
