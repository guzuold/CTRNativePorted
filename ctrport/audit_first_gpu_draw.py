# SPDX-License-Identifier: GPL-2.0-only
import json,hashlib,datetime
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('runtime/user/logs/submit_flip_census.jsonl');rows=list(map(json.loads,source.read_text().splitlines()))
 def one(name):
  found=[r for r in rows if r.get('scope')=='original_GnmSubmitFlip_'+name and (name=='ordered_route_handoff' or r.get('word')==255)];assert len(found)==1;return found[0]
 draw=one('first_GPU_draw');assert draw['word']==255 and draw['status']==0 and draw['invoked'] and draw['real_fence'] and draw['GPU_execution']
 assert draw['vertices']==3 and draw['instances']==1 and draw['vertex_buffers']==2 and draw['completed_packets']>=65 and draw['resume_status']==88
 read=one('first_GPU_readback');assert read['status']==0 and read['writer_generation_completed'] and not read['pixel_payload_persisted'] and read['bytes']==32768
 pending=one('ordered_route_handoff');assert pending['word']>255 and pending['completed_packets']>=65 and pending['status']==88
 release=next(r for r in rows if r.get('scope')=='original_GnmSubmitFlip_ordered_pipeline_release' and r['word']==255);assert release['status']==0 and rows.index(release)>rows.index(read)
 startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
 report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'passed':True,'draw':draw,'readback':read,'pending':pending,'exe_sha256':startup['exe_sha256'],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'scope':'One original retail draw executed and fenced offscreen; no flip or complete game frame','R3':False,'engine_replacements':0}
 Path('reports/reconstruction/infra/first_gpu_draw_retail.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
