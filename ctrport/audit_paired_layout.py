# SPDX-License-Identifier: GPL-2.0-only
"""Actual paired descriptor/pipeline layouts and resource descriptor updates, never GPU binding."""
import datetime,hashlib,json
from pathlib import Path
source=Path('runtime/user/logs/submit_flip_census.jsonl')
events=[json.loads(line) for line in source.read_text().splitlines()]
rows=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_paired_layout']
expected=[x['word'] for x in events if x.get('scope')=='original_GnmSubmitFlip_graphics_pair' and x['status']==0]
assert [x['word'] for x in rows]==expected and len(rows)==39
life=next(x for x in events if x.get('scope')=='original_GnmSubmitFlip_paired_layout_lifecycle')
assert life['status']==life['unsupported']==0 and life['created']==life['released']==life['attempted']==39
updates={e['word']:e for e in events if e.get('scope')=='original_GnmSubmitFlip_image_descriptor_update'}
assert life['fully_written_layouts']==sum(r['descriptor_set_written'] for r in rows)
for row in rows:
 assert row['descriptor_set_written']==(updates[row['word']]['write_status']==0)
 assert row['create_status']==row['release_status']==0
 assert row['release_deferred_to_batch_end']
 assert row['live_buffer_tokens']==58, 'Actual initialized buffers must still exist during layout creation'
 assert not any(row[k] for k in ('pipeline_created','gpu_execution'))
batch=next(x for x in events if x.get('scope')=='original_GnmSubmitFlip_layout_batch')
assert batch['peak_live_layouts']==39 and batch['live_buffers_at_layout_close']==58 and batch['layouts_released_before_buffers'] and batch['status']==0
assert batch['storage_descriptor_updates'] and not batch['descriptor_binding'] and not batch['gpu_execution']
assert batch['live_samplers_at_layout_close']==64 and batch['live_images_at_layout_close']==53
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
exe=Path(startup['exe_path']);assert hashlib.sha256(exe.read_bytes()).hexdigest()==startup['exe_sha256']
manifest=json.loads(exe.with_name(exe.name+'.build.json').read_text(encoding='utf-8-sig'))
assert manifest['executable_sha256']==startup['exe_sha256']
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'retail paired layouts on selected physical session; resource descriptor updates independent, never bound','exe_sha256':startup['exe_sha256'],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'build_input_manifest':manifest,'rows':rows,'lifecycle':life,'batch':batch,'active_stubs':startup['active_stubs'],'called_stubs':startup['called_stubs'],'R3':False,'engine_replacements':0,'GPU_execution':False,'payload_persisted':False}
Path('reports/reconstruction/infra/paired_layout_lifecycle.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(life))
