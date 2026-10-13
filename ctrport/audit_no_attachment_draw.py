# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('runtime/user/logs/submit_flip_census.jsonl');rows=[json.loads(x) for x in source.read_text().splitlines()]
 def event(name,word=3428):
  found=[x for x in rows if x.get('word')==word and x['scope']=='original_GnmSubmitFlip_'+name];assert len(found)==1;return found[0]
 draw=event('no_attachment_GPU_draw');assert draw['status']==0 and draw['invoked'] and draw['GPU_execution'] and draw['real_fence'] and draw['original_key_preserved'] and draw['vertex_buffers']==2 and draw['vertices']==3 and draw['completed_packets']>=761
 key=event('graphics_key');assert key['z_format']==1 and key['mrt_mask']==1 and key['color_count']==1 and len(key['colors'])==8 and all(c['data_format']==c['write_mask']==0 for c in key['colors'])
 pipeline=event('first_pipeline');assert pipeline['status']==0 and pipeline['owned_token']
 resource=event('draw_resource_inputs');assert resource['status']==0 and resource['vertices']==2 and resource['descriptor_complete']
 pending=[x for x in rows if x['scope']=='original_GnmSubmitFlip_ordered_route_handoff'];assert len(pending)==1 and pending[0]['completed_packets']>=761 and pending[0]['status']==88
 flushes=[x for x in rows if x['scope']=='original_GnmSubmitFlip_event_metadata_flush'];assert all(x['status']==0 and x['real_queue_barrier_fence'] and x['GPU_execution'] and not x['guest_writeback'] for x in flushes)
 assert any(x['word']==3469 and x['control']==0x72c and x['depth_dirty_owners']>=2 for x in flushes)
 markers=[x for x in rows if x['scope']=='original_GnmSubmitFlip_optional_debug_marker'];assert all(x['status']==0 and x['public_disabled_branch'] and not x['guest_debug_markers_enabled'] for x in markers)
 assert any(x['word']==3448 and x['marker']==0x68750001 for x in markers)
 controls={name:json.loads(Path('runtime/user/logs/'+file).read_text(encoding='utf-8-sig')) for name,file in [('event_flush','infra_event_metadata_flush_control.json'),('registers','infra_debug_marker_control.json'),('depth_publication','infra_depth_guest_writeback_control.json')]};assert all(x['passed'] for x in controls.values())
 vertex_pending=[x for x in rows if x.get('word')==3615 and x['scope'] in ['original_GnmSubmitFlip_vertex_only_pipeline','original_GnmSubmitFlip_vertex_only_dynamic','original_GnmSubmitFlip_vertex_only_viewport']];assert any(x.get('status')==88 and not x['owned_token'] for x in vertex_pending if x['scope'].endswith('pipeline'))
 startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text());assert startup['exit']==4
 report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,draw=draw,key=key,pipeline=pipeline,resource=resource,pending=pending[0],debug_markers=markers,metadata_flushes=flushes,controls=controls,vertex_pending=vertex_pending,exe_sha256=startup['exe_sha256'],census_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),R3=False,reconstructed_replacements=0,limitations='Actual original3428 draw/fence with no attachments, original shaders/key and real resources. No pixel output is inferred from attachment-free raster execution. Optional Vulkan guest debug markers disabled by explicit runtime policy, whole pinned public false branches. Depth/stencil guest publication remains missing.')
 Path('reports/reconstruction/infra/no_attachment_draw_retail.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(draw=draw,pending=pending[0],debug_markers=len(markers))))
