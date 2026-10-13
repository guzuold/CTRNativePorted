# SPDX-License-Identifier: GPL-2.0-only
import json,hashlib,datetime
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 def result(path):
  rows=[json.loads(line) for line in Path(path).read_text(encoding='utf-8-sig').splitlines() if line.startswith('{')]
  return next(row for row in reversed(rows) if row.get('passed') or row.get('scope')=='authored_color_sampled_gpu_coherence')
 control=result('runtime/user/logs/infra_r32_sampled_transfer_control.log')
 half=result('runtime/user/logs/infra_r32_half_transfer_control.log')
 assert half['R32_FLOAT'] and half['pitched_half_view'] and half['pixel_differences']==half['guard_differences']==0
 completion=result('runtime/user/logs/infra_gpu_resident_WriteData_control.log')
 assert control['R32_FLOAT'] and control['completed_writers']==3 and control['transfers']==3 and control['pixel_differences']==control['guard_differences']==0
 for field in ['post_EOP_WriteData_real_fence','stale_generation_blocks_label','pending_color_blocks_label','WriteData_cannot_overwrite_GPU_backing']:assert completion[field]
 retail=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text());events=retail['events']
 draws=[e for e in events if e.get('scope') in ['original_GnmSubmitFlip_MRT_GPU_draw','original_GnmSubmitFlip_first_GPU_draw','original_GnmSubmitFlip_vertex_only_GPU_draw','original_GnmSubmitFlip_no_attachment_GPU_draw'] and e.get('GPU_execution') and e.get('status')==0]
 words=[e['word'] for e in draws];assert 3903 in words and len(words)==len(set(words))
 refresh=[e for e in events if e.get('scope')=='original_GnmSubmitFlip_MRT_writer_to_sampled' and e.get('word')==2336 and e.get('compared_pixel_bytes')==8294400]
 assert refresh and all(e['status']==0 and e['pixel_differences']==e['padding_differences']==0 for e in refresh)
 pending=[e for e in events if e.get('scope') in ('original_GnmSubmitFlip_ordered_route_handoff','original_GnmSubmitFlip_ordered_submission_complete')][-1]
 if pending.get('scope')=='original_GnmSubmitFlip_ordered_submission_complete':assert pending['status']==0 and not pending['pending'] and pending['completed_packets']==pending['packet_count'] and pending['real_present'] and pending['label_retirement_verified']
 assert pending['word']>=4147 and pending['completed_packets']>=911
 sha=hashlib.sha256(Path(retail['exe_path']).read_bytes()).hexdigest();assert sha==retail['exe_sha256']
 report=dict(scope='retail_R32_sampled_transfer',timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,exe_sha256=sha,control=control,half_control=half,completion_control=completion,retail_refresh=refresh,original_draws=draws,pending=pending,R3=False,engine_replacements=0,limitation='GPU source download and sampled upload each genuinely fenced; raw image bytes compared, component view verified by data. No guest CPU snapshot reuse. Draw cannot repeat after its postdraw failure. No frame/flip claim.')
 Path('reports/reconstruction/infra/r32_sampled_transfer_retail.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(passed=True,pending_word=pending['word'],completed_packets=pending['completed_packets'],original_draws=len(draws),R3=False)))
