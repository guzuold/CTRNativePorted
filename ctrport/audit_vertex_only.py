# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json
from pathlib import Path
source=Path('runtime/user/logs/submit_flip_census.jsonl');events=[json.loads(x) for x in source.read_text().splitlines()]
rows=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_vertex_only']
assert [x['word'] for x in rows]==[680,3242,3615,5851]
for x in rows:
 assert x['status']==0 and x['vertex_spirv_words']>=5 and x['register_words']==0xD000
 assert x['input_join_status']==0 and x['cpu_input_status']==0
 assert not x['fragment_present'] and not x['GPU_execution']
layouts=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_vertex_only_layout']
assert [x['word'] for x in layouts]==[x['word'] for x in rows]
for x in layouts:
 assert all(x[k]==0 for k in ('prepare_status','create_status','buffer_write_status','sampler_write_status','image_write_status'))
 assert x['descriptor_set_written'] and not x['fragment_present'] and not x['GPU_execution']
life=next(x for x in events if x.get('scope')=='original_GnmSubmitFlip_vertex_only_layout_lifecycle')
assert life['created']==life['released']==4 and life['status']==0 and life['released_before_resources']
joins=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_vertex_attachment_join']
assert [x['word'] for x in joins]==[x['word'] for x in rows]
depths={x['word']:x for x in events if x.get('scope')=='original_GnmSubmitFlip_depth_allocation'}
for x in joins:
 d=depths.get(x['word']);assert x['layout_token'] and x['depth_token']==bool(d and not d['create_status'])
 assert x['depth_standalone_initialized']==bool(d and not d['clear_status']) and not x['fragment_present'] and not x['GPU_execution']
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
exe=Path(startup['exe_path']);assert hashlib.sha256(exe.read_bytes()).hexdigest()==startup['exe_sha256']
compiler=Path('build/CTRPort-data/ctr_graphics_companion_v21.dll')
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':startup['exe_sha256'],'compiler_sha256':hashlib.sha256(compiler.read_bytes()).hexdigest(),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'rows':rows,'layouts':layouts,'attachment_joins':joins,'lifecycle':life,'R3':False,'engine_replacements':0,'limitation':'Genuine original PS-address-zero VS compilation with immutable whole original register bank/command/profile. No fabricated PS or skipped draw. Four genuine layouts and null ClipPlanes descriptors retained then released before resources. Pipeline/orderedGPU work not admitted.'}
Path('reports/reconstruction/infra/vertex_only_retail.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'vertex_only_compiled':len(rows),'descriptors':[x['descriptor_count'] for x in rows]}))
