# SPDX-License-Identifier: GPL-2.0-only
"""Reached depth shapes and standalone fenced full-aspect clear; no guest execution."""
import datetime,hashlib,json
from pathlib import Path
source=Path('runtime/user/logs/submit_flip_census.jsonl');events=[json.loads(l) for l in source.read_text().splitlines()]
rows=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_depth_allocation']
lifecycle=next(x for x in events if x.get('scope')=='original_GnmSubmitFlip_depth_allocation_lifecycle')
ordered=json.loads(Path('runtime/user/logs/submit_flip_ordered.json').read_text())
expected=[x for x in ordered['execution_routes'] if x['opcode']!=21 and (x['depth_attachment']['depth_enable'] or x['depth_attachment']['stencil_enable'])]
assert [r['word'] for r in rows]==[r['word'] for r in expected]
for row,route in zip(rows,expected):
 d=route['depth_attachment']
 assert all(row[k]==d[k] for k in ('samples','z_format','stencil_format'))
 assert row['width']==d['extent_hint_width'] and row['height']==d['extent_hint_height']
 assert row['extent_hint_valid'] and d['extent_hint_valid'] and d['slice_start']==d['slice_max']==0
 assert row['create_status']==row['release_status']==0 and not row['gpu_execution']
 eligible=d['depth_clear_enable'] and (not d['stencil_format'] or d['stencil_clear_enable'])
 assert row['clear_status']==(0 if eligible else 88)
 assert row['undefined_contents']==(not eligible)
 assert row['clear_float_bits']==d['depth_clear_float_bits'] and row['stencil_clear']==d['stencil_clear_raw']
assert [r['word'] for r in rows if r['clear_status']==0]==[680,3242,5851]
assert lifecycle['status']==lifecycle['unsupported']==0 and lifecycle['attempted']==lifecycle['created']==lifecycle['released']==len(rows)==8
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text());exe=Path(startup['exe_path']);assert hashlib.sha256(exe.read_bytes()).hexdigest()==startup['exe_sha256']
manifest_path=exe.with_name(exe.name+'.build.json')
manifest=json.loads(manifest_path.read_text(encoding='utf-8-sig'));assert manifest['executable_sha256']==startup['exe_sha256']
registry=json.loads(Path('ctrport/compat_stubs.json').read_text())['entries']
called_indices=sorted({c['registry_index'] for c in startup['called_stubs']})
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'retail reached depth shapes and standalone full-aspect fenced initialization; no guest LOAD/HTILE admission',
 'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'exe_sha256':startup['exe_sha256'],'build_input_manifest':manifest,'active_stubs':len(registry),'called_stubs':[registry[i] for i in called_indices],'rows':rows,'lifecycle':lifecycle,
 'R3':False,'engine_replacements':0,'standalone_cleared_images':3,'undefined_images':5,'guest_HTILE':False,'guest_commands_consumed':0,'GPU_execution':False,'payload_persisted':False}
Path('reports/reconstruction/infra/depth_allocation_lifecycle.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(lifecycle))
