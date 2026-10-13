# SPDX-License-Identifier: GPL-2.0-only
import datetime, hashlib, json, subprocess
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('reports/reconstruction/infra/shareplay_pending_startup.json')
 retail=json.loads(source.read_text());events=retail['events']
 def rows(scope,word=None):return [e for e in events if e.get('scope')==scope and (word is None or e.get('word')==word)]
 assert retail['exit']==4 and not retail['R3'] and retail['engine_replacements']==0
 exe=Path(retail['exe_path']);assert hashlib.sha256(exe.read_bytes()).hexdigest()==retail['exe_sha256']
 rect=rows('original_GnmSubmitFlip_vertex_only_GPU_draw',5851)
 assert len(rect)==1 and rect[0]['status']==0 and rect[0]['real_fence'] and rect[0]['GPU_execution']
 draw=rows('original_GnmSubmitFlip_MRT_GPU_draw',6052)
 assert len(draw)==1 and draw[0]['status']==0 and draw[0]['GPU_execution']
 published=rows('original_GnmSubmitFlip_color_guest_writeback',6052)
 assert len(published)==1 and published[0]['status']==0 and published[0]['guest_bytes']==2359296 and published[0]['actual_GPU_retiled_generation'] and not published[0]['poisoned']
 readback=rows('original_GnmSubmitFlip_MRT_GPU_readback',6052)
 assert readback and all(not e['sample0_reverse_fenced'] for e in readback)
 captured=rows('original_GnmSubmitFlip_D32_single_pre_writer_capture',5851)
 assert len(captured)==1 and captured[0]['span_status']==captured[0]['capture_status']==0 and captured[0]['before_GPU_clear'] and captured[0]['depth_bytes']==2621440 and captured[0]['stencil_bytes']==552960 and not captured[0]['pixel_export_provider']
 pending=(rows('original_GnmSubmitFlip_ordered_route_handoff')+rows('original_GnmSubmitFlip_ordered_submission_complete'))[-1]
 if pending.get('scope')=='original_GnmSubmitFlip_ordered_submission_complete':assert pending['status']==0 and not pending['pending'] and pending['completed_packets']==pending['packet_count'] and pending['real_present'] and pending['label_retirement_verified']
 assert pending['word']>=6309 and pending['completed_packets']>=1409
 log=Path('runtime/user/logs/infra_single_r32_depth_control.log')
 control=[json.loads(x) for x in log.read_text().splitlines() if x.startswith('{')][-1]
 assert control['passed'] and control['differences']==0 and control['repeated_writer_stale_rejected']
 baseline=subprocess.run([str(exe),'--published-color-generations-control'],capture_output=True,text=True,timeout=10)
 assert baseline.returncode==0
 result=dict(scope='retail_sample1_original_draw_chain',timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,exe_sha256=retail['exe_sha256'],source_report_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),rect=rect,draw=draw,published=published,pre_writer_capture=captured,pending=pending,authored_GPU_control=control,baseline_control=json.loads(baseline.stdout),R3=False,engine_replacements=0,limitation='SharePlay parked diagnostic; no normal cold boot/frame. Sample1 depth has captured original pre-writer planes, no pixel export or EOP residency claim.')
 Path('reports/reconstruction/infra/sample1_draw_chain_retail.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps(dict(passed=True,pending_word=pending['word'],completed_packets=pending['completed_packets'],R3=False)))
