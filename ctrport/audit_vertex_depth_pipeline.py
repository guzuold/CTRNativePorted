# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('runtime/user/logs/submit_flip_census.jsonl')
 rows=[json.loads(x) for x in source.read_text().splitlines()]
 def events(name):return [r for r in rows if r.get('scope')=='original_GnmSubmitFlip_'+name]
 auxiliary=events('vertex_only_auxiliary');pipeline=events('vertex_only_pipeline');release=events('vertex_only_pipeline_release')
 assert len(auxiliary)==len(pipeline)==len(release)==1
 a,p,r=auxiliary[0],pipeline[0],release[0]
 assert a['word']==p['word']==r['word']==680 and a['status']==p['status']==r['status']==0
 assert (a['control_words'],a['evaluation_words'])==(575,239) and a['owned_after_DLL_unload'] and a['fragment_absent']
 assert p['owned_token'] and p['fragment_absent'] and not p['GPU_execution'] and r['before_layouts']
 assert rows.index(a)<rows.index(p)<rows.index(r)
 draw=events('vertex_only_GPU_draw')[0]
 assert draw['word']==680 and draw['status']==0 and draw['real_fence'] and draw['GPU_execution'] and draw['vertices']==3 and draw['instances']==1 and draw['completed_packets']==174
 assert rows.index(p)<rows.index(draw)<rows.index(r)
 pending=events('ordered_route_handoff')[0]
 assert pending['word']>983 and pending['completed_packets']>=231 and pending['status']==88
 acquire=[r for r in events('acquire_mem_request') if r['word']==690]
 assert len(acquire)==1 and acquire[0]['status']==0 and acquire[0]['real_provider_missing'] and not acquire[0]['GPU_execution']
 startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
 report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,auxiliary=a,pipeline=p,release=r,draw=draw,next_acquire_mem=acquire[0],pending=pending,exe_sha256=startup['exe_sha256'],census_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),R3=False,engine_replacements=0,scope='Actual original680 RectList depth/MSAA2 pipeline and synchronous draw/fence; cursor past original compute983. Whole public auxiliary emission owned after DLL unload; raw AcquireMem transport is not a cache implementation.')
 Path('reports/reconstruction/infra/vertex_depth_pipeline_retail.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report))
