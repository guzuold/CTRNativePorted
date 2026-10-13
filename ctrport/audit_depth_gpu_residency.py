# SPDX-License-Identifier: GPL-2.0-only
import json, hashlib, datetime
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('reports/reconstruction/infra/shareplay_pending_startup.json')
 report=json.loads(source.read_text()); events=report['events']
 def rows(scope):return [e for e in events if e.get('scope')==scope]
 resident=rows('original_GnmSubmitFlip_depth_GPU_resident')
 eop=rows('original_GnmSubmitFlip_EOP_completion')
 cleanup=rows('original_GnmSubmitFlip_resident_after_original_release')
 pending=(rows('original_GnmSubmitFlip_ordered_route_handoff')+rows('original_GnmSubmitFlip_ordered_submission_complete'))[-1]
 if pending.get('scope')=='original_GnmSubmitFlip_ordered_submission_complete':assert pending['status']==0 and not pending['pending'] and pending['completed_packets']==pending['packet_count'] and pending['real_present'] and pending['label_retirement_verified']
 assert report['exit']==4 and not report['R3'] and report['engine_replacements']==0
 assert resident and all(e['status']==0 and e['CPU_padding_bytes']==3072 and e['depth_bytes']==17694720 and e['stencil_bytes']==2073600 and not e['guest_publication'] for e in resident)
 assert eop[-1]['word']==3622 and eop[-1]['status']==0 and eop[-1]['label_written_and_verified'] and not eop[-1]['IRQ_delivered']
 assert cleanup and all(e['status']==0 and e['real_sampled_alias'] and e['CPU_guard_retained'] and e['exact_logical_spans_resolved'] and e['partial_and_stale_lookup_rejected'] for e in cleanup)
 assert pending['word']>=3903 and pending['completed_packets']>=855
 sha=hashlib.sha256(Path(report['exe_path']).read_bytes()).hexdigest();assert sha==report['exe_sha256']
 output=dict(scope='retail_depth_GPU_residency',timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,exe_sha256=sha,source_report_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),resident=resident,EOP=eop,after_original_release=cleanup,next_pending=pending,retail_residency_policy_active=True,R3=False,engine_replacements=0,limitation='No frame. Exact depth/stencil logical spans retained on actual GPU; rounded page padding is CPU exclusion only, no stencil export or HTILE mapping. CPU access remains explicit missing.')
 Path('reports/reconstruction/infra/depth_GPU_residency_retail.json').write_text(json.dumps(output,indent=2)+'\n')
 print(json.dumps(dict(passed=True,pending_word=pending['word'],completed_packets=pending['completed_packets'],R3=False)))
