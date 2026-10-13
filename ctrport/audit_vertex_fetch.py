# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json,collections
from pathlib import Path
source=Path('runtime/user/logs/submit_flip_census.jsonl')
events=[json.loads(x) for x in source.read_text().splitlines()]
rows=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_vertex_fetch']
assert len(rows)==43 and len({x['word'] for x in rows})==43
assert all(x['format_status']==0 and not x['physical_vertex_format_authority'] for x in rows)
assert all(not x['GPU_execution'] and x['status'] in (0,88) for x in rows)
assert all(x['captured_buffers']==x['attribute_count'] for x in rows if x['status']==0)
preflight=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_vertex_preflight']
assert len(preflight)==43 and {x['word'] for x in preflight}=={x['word'] for x in rows}
assert all(x['status'] in (0,88) and x['selected_session'] and not x['GPU_execution'] for x in preflight)
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':startup['exe_sha256'],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'companion_sha256':hashlib.sha256(Path('build/CTRPort-data/ctr_graphics_companion_v24.dll').read_bytes()).hexdigest(),'rows':rows,'physical_preflight':preflight,'physical_statuses':dict(collections.Counter(x['status'] for x in preflight)),'statuses':dict(collections.Counter(x['status'] for x in rows)),'captured_bytes':sum(x['captured_bytes'] for x in rows),'R3':False,'engine_replacements':0,'limitation':'Whole public GetVertexInputs on exact same-pass userdata/fetch metadata; effective sharp leases and immutable CPU byte snapshots retained. Selected-session physical vertex format/limit preflight reported separately; no vertex buffer binding or draw.'}
Path('reports/reconstruction/infra/vertex_fetch_retail.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'statuses':report['statuses'],'captured_bytes':report['captured_bytes']}))
