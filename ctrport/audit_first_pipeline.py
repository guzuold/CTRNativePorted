# SPDX-License-Identifier: GPL-2.0-only
import json,hashlib,datetime
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('runtime/user/logs/submit_flip_census.jsonl')
 rows=[json.loads(x) for x in source.read_text().splitlines()]
 created=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_first_pipeline' and x.get('word')==255]
 released=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_ordered_pipeline_release' and x.get('word')==255]
 assert len(created)==len(released)==1
 assert created[0]['word']==255 and created[0]['status']==0 and created[0]['owned_token'] and not created[0]['GPU_execution']
 assert released[0]['status']==0
 read=next(x for x in rows if x.get('scope')=='original_GnmSubmitFlip_first_GPU_readback' and x['word']==255)
 assert rows.index(released[0])>rows.index(read)
 pending=next(x for x in rows if x.get('scope')=='original_GnmSubmitFlip_ordered_route_handoff')
 assert pending['status']==88
 gpu=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_first_GPU_draw']
 completed=bool(gpu and gpu[0]['status']==0)
 assert (pending['completed_packets']>=65 if completed else pending['completed_packets']==64) and (pending['word']>255 if completed else pending['word']==255)
 report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'passed':True,'created':created,'released':released,'pending':pending,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'executable_sha256':hashlib.sha256(Path('build/CTRPort-infra/ctrport.exe').read_bytes()).hexdigest(),'R3':False,'reconstructed_replacements':0}
 Path('reports/reconstruction/infra/first_pipeline_retail.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report))
