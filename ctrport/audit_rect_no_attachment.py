# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('reports/reconstruction/infra/shareplay_pending_startup.json');startup=json.loads(source.read_text());rows=startup['events']
 def event(name,word=3615):
  found=[r for r in rows if r.get('scope')=='original_GnmSubmitFlip_'+name and r.get('word')==word];assert len(found)==1;return found[0]
 draw=event('vertex_only_GPU_draw');assert draw['status']==0 and draw['invoked'] and draw['real_fence'] and draw['GPU_execution'] and draw['vertices']==3 and draw['instances']==1 and draw['completed_packets']==794 and not draw['guest_depth_writeback']
 pipeline=event('vertex_only_pipeline');assert pipeline['status']==0 and pipeline['owned_token'] and pipeline['fragment_absent']
 viewport=event('vertex_only_viewport');assert (viewport['width'],viewport['height'],viewport['scissor_width'],viewport['scissor_height'])==(16384,16384,1024,1024)
 dynamic=event('vertex_only_dynamic');assert dynamic['decode_status']==0 and not any(dynamic[k] for k in ['depth_test','stencil_test','depth_write','depth_bounds_test','depth_bias','rasterizer_discard'])
 key=event('graphics_key');assert key['z_format']==1 and key['mrt_mask']==0 and key['color_count']==0 and key['primitive_type']==17 and key['samples']==key['depth_samples']==1 and not key['fragment_present']
 eop=event('EOP_completion',3622);assert eop['status']==88 and eop['event_control']==0x504 and eop['prior_color_writers']==0 and eop['prior_depth_writers']==2 and not eop['label_written_and_verified'] and not eop['IRQ_delivered'] and not eop['poisoned']
 pending=[r for r in rows if r.get('scope')=='original_GnmSubmitFlip_ordered_route_handoff'];assert len(pending)==1 and pending[0]['word']==3622 and pending[0]['completed_packets']==794 and pending[0]['status']==88
 cleanup=[r for r in rows if r.get('scope','').endswith('_lifecycle') and r.get('scope','').startswith('original_GnmSubmitFlip_')];assert cleanup and all(r.get('status')==0 for r in cleanup)
 draws=[r for r in rows if 'GPU_draw' in r.get('scope','') and r.get('GPU_execution')];assert len(draws)==12 and all(r['status']==0 for r in draws)
 controls={name:next(json.loads(line) for line in reversed(Path('runtime/user/logs/'+file).read_text(encoding='utf-8-sig').splitlines()) if line.startswith('{') and json.loads(line).get('passed')) for name,file in [('EOP','infra_eop_image_gate_control.json'),('bounded_event_GPU','infra_eop_bounded_gpu_control.json'),('kernel_GPU','infra_eop_image_gate_gpu_control.json'),('large_depth','infra_depth_large_control.json'),('depth_capture','infra_depth_guest_writeback_control.json')]};assert all(r['passed'] for r in controls.values())
 assert startup['exit']==4
 report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,draw=draw,pipeline=pipeline,key=key,viewport=viewport,dynamic=dynamic,EOP=eop,pending=pending[0],controls=controls,lifecycle=cleanup,exe_sha256=startup['exe_sha256'],source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),original_draws=len(draws),draw_words=[r['word'] for r in draws],original_computes=10,active_R8_stubs=19,called_R8_stubs=17,R3=False,reconstructed_replacements=0,limitations='Original3615 executes genuine vertex-only RectList with original key and unclamped viewport. No pixel output from attachment-free execution. Original EOP3622 remains blocked on two unpublished depth owners; no label/IRQ/flip. Full-size depth transport control has authored bytes and synthetic downloads, not GPU depth publication.')
 Path('reports/reconstruction/infra/rect_no_attachment_retail.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(passed=True,draw_word=3615,pending=3622,completed_packets=794,R3=False)))
