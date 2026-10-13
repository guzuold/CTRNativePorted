# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('reports/reconstruction/infra/shareplay_pending_startup.json');r=json.loads(source.read_text());events=r['events']
 def rows(scope,word=None):return [e for e in events if e.get('scope')==scope and (word is None or e.get('word')==word)]
 assert r['exit']==4 and not r['R3'] and r['engine_replacements']==0
 assert hashlib.sha256(Path(r['exe_path']).read_bytes()).hexdigest()==r['exe_sha256']
 admission=rows('original_GnmSubmitFlip_single_MS2_inactive_depth_admission',6309)
 assert len(admission)==1 and admission[0]['depth_attachment_absent'] and admission[0]['original_key_preserved'] and admission[0]['depth_test']==admission[0]['stencil_test']==0
 draw=rows('original_GnmSubmitFlip_MRT_GPU_draw',6309);assert len(draw)==1 and draw[0]['status']==0 and draw[0]['GPU_execution']
 readback=rows('original_GnmSubmitFlip_MRT_GPU_readback',6309);assert readback and all(x['status']==0 and x['sample0_reverse_fenced'] for x in readback)
 publication=rows('original_GnmSubmitFlip_color_guest_writeback',6309);assert publication and all(x['status']==0 and x['actual_GPU_retiled_generation'] and not x['poisoned'] for x in publication)
 pending=(rows('original_GnmSubmitFlip_ordered_route_handoff')+rows('original_GnmSubmitFlip_ordered_submission_complete'))[-1];assert pending['word']>=10323 and pending['completed_packets']>=2338
 if pending.get('scope')=='original_GnmSubmitFlip_ordered_submission_complete':assert pending['status']==0 and not pending['pending'] and pending['completed_packets']==pending['packet_count'] and pending['real_present'] and pending['label_retirement_verified']
 draws=[e for e in events if e.get('GPU_execution') and 'GPU_draw' in e.get('scope','')];assert len(draws)>=43
 controls=[json.loads(x) for x in Path('runtime/user/logs/infra_single_ms2_inactive_control.log').read_text().splitlines() if x.startswith('{')];assert controls[-1]['passed'] and controls[-1]['differences']==0
 result=dict(scope='retail_inactive_depth_single_color_MS2',timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,exe_sha256=r['exe_sha256'],source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),admission=admission,draw=draw,readback=readback,publication=publication,original_draws=len(draws),color_guest_bytes=sum(x.get('guest_bytes',0) for x in rows('original_GnmSubmitFlip_color_guest_writeback') if x['status']==0),pending=pending,authored_GPU_control=controls[-1],R3=False,engine_replacements=0,limitation='Parked SharePlay diagnostic; no frame/normal cold boot. Sample1 EOP path prepared, not reached or claimed.')
 Path('reports/reconstruction/infra/inactive_ms2_draw_retail.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(passed=True,draws=len(draws),pending=pending)))
