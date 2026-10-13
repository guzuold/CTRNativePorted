# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('runtime/user/logs/submit_flip_census.jsonl');rows=[json.loads(x) for x in source.read_text().splitlines()]
 def event(scope,word):
  found=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_'+scope and x.get('word')==word];assert len(found)==1;return found[0]
 draw=event('vertex_only_GPU_draw',3242);assert draw['status']==0 and draw['invoked'] and draw['GPU_execution'] and draw['real_fence'] and draw['completed_packets']==756 and draw['vertices']==3 and draw['instances']==1
 pipeline=event('vertex_only_pipeline',3242);assert pipeline['status']==0 and pipeline['owned_token'] and pipeline['fragment_absent']
 allocation=event('depth_allocation',3242);assert (allocation['width'],allocation['height'],allocation['samples'],allocation['z_format'],allocation['stencil_format'])==(1024,1024,1,1,0) and allocation['create_status']==allocation['clear_status']==0
 dynamic=event('vertex_only_dynamic',3242);assert dynamic['decode_status']==0 and not dynamic['depth_bias'] and not dynamic['primitive_restart']
 pending=event('ordered_route_handoff',3428);assert pending['status']==88 and pending['completed_packets']==756
 report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,draw=draw,pipeline=pipeline,allocation=allocation,dynamic=dynamic,pending=pending,exe_sha256=hashlib.sha256(Path('build/CTRPort-infra/ctrport.exe').read_bytes()).hexdigest(),census_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),R3=False,reconstructed_replacements=0,limitations='Actual original D16 rectangle draw/fence. Depth/stencil guest publication still missing, no frame or flip. Generic dynamic preflight is not depth-only draw validation.')
 Path('reports/reconstruction/infra/d16_rect_retail.json').write_text(json.dumps(report,indent=2)+'\n');print('Original3242 D16 single-sample rectangle draw/fence passed; pending3428')
