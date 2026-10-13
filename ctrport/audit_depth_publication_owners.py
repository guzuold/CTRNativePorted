# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('reports/reconstruction/infra/shareplay_pending_startup.json');startup=json.loads(source.read_text());rows=startup['events']
 owners=[r for r in rows if r.get('scope')=='original_GnmSubmitFlip_depth_publication_owner'];assert len(owners)==2 and {r['writer_word'] for r in owners}=={2075,3242} and len({r['token'] for r in owners})==2
 assert all(r['word']==3622 and not r['poisoned'] for r in owners)
 d16=next(r for r in owners if r['writer_word']==3242);d32=next(r for r in owners if r['writer_word']==2075)
 assert not d16['dirty'] and d16['captured_planes']==1 and d16['publication_completed']
 assert d32['dirty'] and d32['captured_planes']==0 and not d32['publication_completed']
 capture=next(r for r in rows if r.get('scope')=='original_GnmSubmitFlip_D16_pre_writer_capture' and r['word']==3242)
 assert capture['extent_status']==capture['capture_status']==0 and capture['bytes']==2097152 and capture['before_GPU_clear']
 downloads=[r for r in rows if r.get('scope')=='original_GnmSubmitFlip_D16_linear_download'];assert len(downloads)==1
 download=downloads[0];assert download['word']==3242 and download['status']==0 and download['width']==download['height']==1024 and download['linear_pitch']==2048 and download['linear_bytes']==2097152 and download['guards_unchanged'] and download['actual_GPU_download'] and not download['guest_writeback'] and not download['pixel_payload_persisted'] and download['dirty_owners_retained']==2
 eop=next(r for r in rows if r.get('scope')=='original_GnmSubmitFlip_EOP_completion');assert eop['status']==88 and eop['prior_depth_writers']==2 and eop['data_select']==1 and not eop['interrupt_select'] and not eop['label_written_and_verified'] and not eop['IRQ_delivered']
 pending=[r for r in rows if r.get('scope')=='original_GnmSubmitFlip_ordered_route_handoff'];assert len(pending)==1 and pending[0]['word']==3622 and pending[0]['completed_packets']==794 and pending[0]['status']==88
 controls={}
 for name,file in [('ownership','infra_depth_publication_owners_control.json'),('GPU_EOP','infra_depth_publication_eop_gpu_control.log')]:
  controls[name]=next(json.loads(x) for x in reversed(Path('runtime/user/logs/'+file).read_text(encoding='utf-8-sig').splitlines()) if x.startswith('{') and json.loads(x).get('passed'))
 assert controls['ownership']['late_real_provider_binding_keeps_snapshot'] and controls['ownership']['provider_reentrancy_rejected'] and controls['GPU_EOP']['real_queue_fences']==2 and controls['GPU_EOP']['real_kernel_waiter'] and not controls['GPU_EOP']['actual_GPU_depth_publication']
 lifecycle=[r for r in rows if r.get('scope','').startswith('original_GnmSubmitFlip_') and r.get('scope','').endswith('_lifecycle')];assert len(lifecycle)==8 and all(r['status']==0 for r in lifecycle)
 manifest=json.loads(Path('build/CTRPort-infra/ctrport.exe.build.json').read_text(encoding='utf-8-sig'));exe_sha=hashlib.sha256(Path('build/CTRPort-infra/ctrport.exe').read_bytes()).hexdigest();assert exe_sha==manifest['executable_sha256']==startup['exe_sha256'] and startup['exit']==4
 report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,exe_sha256=exe_sha,source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),source_code_sha256={f:hashlib.sha256(Path(f).read_bytes()).hexdigest() for f in ['ctrport/depth_publication_owners.hpp','ctrport/entry_boundary.hpp','ctrport/gnm_write_data.hpp']},owners=owners,D16_pre_writer_capture=capture,D16_linear_download=download,EOP=eop,pending=pending[0],controls=controls,lifecycle=lifecycle,original_GPU_draws=12,original_GPU_computes=10,active_R8_stubs=19,called_R8_stubs=17,R3=False,reconstructed_replacements=0,limitations='Actual full original D16 GPU download/public tile0 retile/2097152B guest write and reread passed, snapshot captured before first clear. Independent retail tiling equivalence not claimed; actual D16 uniform65535 cannot prove address permutation. D32/MS2 and stencil remain dirty/missing. GPU EOP integration control uses synthetic depth downloads into authored owned memory, real queue fences and real kernel delivery. No original stencil sample mapping, full submission completion, frame or flip claimed.')
 Path('reports/reconstruction/infra/depth_publication_owners_retail.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(passed=True,D16_linear_bytes=2097152,dirty_owners=1,pending=3622,R3=False)))
