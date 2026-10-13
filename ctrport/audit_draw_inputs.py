# SPDX-License-Identifier: GPL-2.0-only
import json,hashlib,datetime,collections
from pathlib import Path
source=Path('runtime/user/logs/submit_flip_census.jsonl');events=list(map(json.loads,source.read_text().splitlines()))
packets=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_draw_packet_inputs']
resources=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_draw_resource_inputs']
assert len(packets)==len(resources)==43 and {x['word'] for x in packets}=={x['word'] for x in resources}
assert all(x['status']==0 and x['opcode'] in (0x27,0x2d) and not x['GPU_execution'] for x in packets)
assert sum(x['indexed'] for x in packets)==2
assert all(x['status'] in (0,88) and not x['GPU_execution'] for x in resources)
first=next(x for x in resources if x['word']==255);assert first['status']==0 and first['vertices']==2 and first['descriptor_complete']
pending=[]
for row in resources:
 if not row['status']:continue
 joins=[x for x in events if x.get('word')==row['word'] and x.get('scope') in ('original_GnmSubmitFlip_attachment_join','original_GnmSubmitFlip_vertex_attachment_join')]
 assert len(joins)==1
 pending.append({'word':row['word'],'descriptor_complete':row['descriptor_complete'],'attachment_join':joins[0]})
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
r={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':startup['exe_sha256'],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'packets':packets,'resource_joins':resources,'pending_attachment_evidence':pending,'statuses':dict(collections.Counter(x['status'] for x in resources)),'R3':False,'engine_replacements':0,'limitation':'All original draw packet fields retained; resource dependencies joined within selected owner callback. Missing descriptors/attachments remain88. No pipeline, dynamic state application, render begin, LOAD or GPU draw completion.'}
Path('reports/reconstruction/infra/draw_inputs_retail.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r['statuses']))
