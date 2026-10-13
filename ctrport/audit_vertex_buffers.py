# SPDX-License-Identifier: GPL-2.0-only
import json,hashlib,datetime
from pathlib import Path
source=Path('runtime/user/logs/submit_flip_census.jsonl');events=list(map(json.loads,source.read_text().splitlines()))
rows=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_vertex_buffer']
joins=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_vertex_layout_join']
lifecycle=next(x for x in events if x.get('scope')=='original_GnmSubmitFlip_vertex_buffer_lifecycle')
assert len(rows)==len(joins)==75 and len({x['word'] for x in rows})==39
assert all(x['status']==x['differences']==0 and x['retained'] and x['usage']==4 and not x['GPU_execution'] for x in rows)
assert all(x['unique_layout'] for x in joins)
assert lifecycle['created']==lifecycle['released']==lifecycle['live_joins']==75 and lifecycle['compared_bytes']==105712 and lifecycle['status']==0 and lifecycle['released_after_layouts']
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
r={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':startup['exe_sha256'],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'rows':rows,'joins':joins,'lifecycle':lifecycle,'R3':False,'engine_replacements':0,'limitation':'Actual selected-session vertex buffer allocations from original effective-sharp snapshots, host initial-byte comparison and per-draw semantic/layout joins; no pipeline bind, draw or game writer coherence.'}
Path('reports/reconstruction/infra/vertex_buffers_retail.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(lifecycle))
