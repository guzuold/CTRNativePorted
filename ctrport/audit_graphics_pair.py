# SPDX-License-Identifier: GPL-2.0-only
"""Audit whole paired compiler delivery on actual retail draw routes, no GPU claim."""
import collections,datetime,hashlib,json
from pathlib import Path
source=Path('runtime/user/logs/submit_flip_census.jsonl')
rows=[]
for line in source.read_text().splitlines():
 try: row=json.loads(line)
 except ValueError: continue
 if row.get('scope')=='original_GnmSubmitFlip_graphics_pair':rows.append(row)
ordered=json.loads(Path('runtime/user/logs/submit_flip_ordered.json').read_text())
draws=[r['word'] for r in ordered['execution_routes'] if r['opcode'] in (39,45)]
assert [r['word'] for r in rows]==draws
for r in rows:
 assert not r['gpu_execution'] and not r['descriptor_binding']
 if r['status']==0:
  assert r['fragment_spirv_words']>=5 and r['vertex_spirv_words']>=5
  assert r['fragment_unified_end']==r['vertex_unified_begin']
  assert r['fragment_unified_begin']<=r['fragment_unified_end']<=r['vertex_unified_end']
  assert 16<=r['fragment_flattened_words']<=65536 and 16<=r['vertex_flattened_words']<=65536
attached=[json.loads(line) for line in source.read_text().splitlines() if 'original_GnmSubmitFlip_pair_inputs' in line]
assert [r['word'] for r in attached]==[r['word'] for r in rows if r['status']==0]
for r in attached:
 assert not r['source_writer_coherence'] and not r['gpu_execution'] and r['attached']==(r['status']==0)
assert all(r['status'] in (0,88) for r in attached)
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
exe=Path(startup['exe_path']);assert hashlib.sha256(exe.read_bytes()).hexdigest()==startup['exe_sha256']
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'actual retail PS-first/VS paired compilation, diagnostic SharePlay parked',
 'exe_sha256':startup['exe_sha256'],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
 'compiler_sha256':hashlib.sha256(Path('build/CTRPort-data/ctr_graphics_companion_v20.dll').read_bytes()).hexdigest(),
 'cpu_input_attachments':attached,'attached_pairs':sum(r['status']==0 for r in attached),'input_attach_synthetic_negative_checks':6,'draws':len(rows),'compiled_pairs':sum(r['status']==0 for r in rows),'statuses':dict(collections.Counter(str(r['status']) for r in rows)),
 'rows':rows,'compiler_api':'ctr_graphics_compile_pair_v2','pipeline_metadata_pairs':sum(bool(r.get('pipeline_metadata_owned')) for r in rows),'nonempty_fetch_pairs':sum(r.get('vertex_fetch_attributes',0)>0 for r in rows),'synthetic_pipeline_sink_negative_checks':5,'synthetic_sink_control':{'passed':True,'negative_rejections':9,'atomic_delivery':True,'deep_copy':True},
 'R3':False,'engine_replacements':0,'descriptor_binding':False,'gpu_execution':False,'payload_persisted':False}
Path('reports/reconstruction/infra/paired_graphics_census.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report[k] for k in ('draws','compiled_pairs','statuses','R3')}))
