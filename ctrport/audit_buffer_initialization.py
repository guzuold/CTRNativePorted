# SPDX-License-Identifier: GPL-2.0-only
"""Actual retail CPU-owned input initialization of selected-device VkBuffers."""
import collections,datetime,hashlib,json
from pathlib import Path
source=Path('runtime/user/logs/submit_flip_census.jsonl')
events=[json.loads(line) for line in source.read_text().splitlines()]
rows=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_buffer_initialization']
lifecycle=next(x for x in events if x.get('scope')=='original_GnmSubmitFlip_buffer_lifecycle')
assert lifecycle['status']==0 and lifecycle['attempted']==lifecycle['created']==lifecycle['released']==len(rows)
assert lifecycle['peak_live_buffers']==len(rows), 'All actual buffers must coexist until batch close'
assert lifecycle['owner_thread_dispatch'] and not any(lifecycle[x] for x in ('descriptor_binding','gpu_readback','gpu_execution'))
for row in rows:
 assert row['bytes']>0 and row['usage'] in (1,2,3)
 assert all(row[k]==0 for k in ('create_status','copy_status','release_status','differences'))
 assert row['host_initialization_only'] and not row['gpu_execution']
 assert row['release_deferred_to_batch_end']
assert lifecycle['host_compared_bytes']==sum(x['bytes'] for x in rows)
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
stages=[e['owned_stage'] for e in startup['events'] if e.get('scope')=='original_GnmSubmitFlip_graphics_census' and 'owned_stage' in e]
stages += [e['owned_compiled_stage'] for e in startup['events'] if e.get('scope')=='original_GnmSubmitFlip_first_graphics' and 'owned_compiled_stage' in e]
expected=[]
for stage in stages:
 expected += [(b['bytes'],1,b['sharp_index']) for b in stage['buffers'] if b['capture_status']==0 and b['copied_bytes']]
 expected += [(b['bytes'],1,b['sharp_index']) for b in stage['flatbufs'] if b['status']==0 and b['owned_words']]
actual=[(r['bytes'],r['usage'],r['sharp_index']) for r in rows if r['usage']==1]
assert collections.Counter(actual)==collections.Counter(expected)
indices=json.loads(Path('reports/reconstruction/infra/index_capture_census.json').read_text())
assert sum(x['bytes'] for x in rows if x['usage'] in (2,3))==408
assert sum(x['usage'] in (2,3) for x in rows)==2
exe=Path(startup['exe_path']);assert hashlib.sha256(exe.read_bytes()).hexdigest()==startup['exe_sha256']
manifest_path=exe.with_name(exe.name+'.build.json')
manifest=json.loads(manifest_path.read_text(encoding='utf-8-sig'));assert manifest['executable_sha256']==startup['exe_sha256']
registry=json.loads(Path('ctrport/compat_stubs.json').read_text())['entries']
called_indices=sorted({c['registry_index'] for c in startup['called_stubs']})
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'actual selected-device host-visible coherent buffer initialization, diagnostic retail SharePlay parked',
 'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'exe_sha256':startup['exe_sha256'],'build_input_manifest':manifest,'active_stubs':len(registry),'called_stubs':[registry[i] for i in called_indices],'lifecycle':lifecycle,'usage_counts':dict(collections.Counter(str(r['usage']) for r in rows)),
 'rows':rows,'host_initialization_comparison':True,'GPU_readback':False,'descriptor_binding':False,'writer_coherence':False,'R3':False,'engine_replacements':0,'payload_persisted':False}
Path('reports/reconstruction/infra/buffer_initialization_lifecycle.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(lifecycle))
