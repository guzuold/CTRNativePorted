# SPDX-License-Identifier: GPL-2.0-only
import json,hashlib,datetime
from pathlib import Path
from ctrport.retail_pending import ordered_pending
source=Path('runtime/user/logs/submit_flip_census.jsonl');events=list(map(json.loads,source.read_text().splitlines()))
rows=ordered_pending(events)
assert len(rows)==1
row=rows[0];assert row['word']>983 and row['status']==88 and row['word']>=2574 and row['completed_packets']>=231 and not row['GPU_execution']
mrt=next(x for x in events if x.get('scope')=='original_GnmSubmitFlip_pair_color_inputs' and x.get('word')==1377);assert mrt['status']==0 and mrt['owned_colors']==2 and mrt['mrt_mask']==3
pipeline=next(x for x in events if x.get('scope')=='original_GnmSubmitFlip_first_pipeline' and x.get('word')==1377);assert pipeline['status']==0 and pipeline['owned_token'] and not pipeline['GPU_execution']
copies=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_color_MS_backing_copy' and x.get('word')==1377]
assert len(copies)==2 and [x['slot'] for x in copies]==[0,1] and all(x['status']==0 and x['direction']==0 and x['original_key_unchanged'] and x['GPU_execution'] and not x['guest_writeback'] for x in copies)
draw_pending=next(x for x in events if x.get('scope')=='original_GnmSubmitFlip_MRT_GPU_draw' and x.get('word')==1377)
assert draw_pending['status']==0 and draw_pending['invoked'] and draw_pending['GPU_execution'] and draw_pending['completed_packets']==317
gpu=next(x for x in events if x.get('scope')=='original_GnmSubmitFlip_first_GPU_draw');assert gpu['status']==0 and gpu['real_fence'] and gpu['GPU_execution']
dispatches=[x for x in events if x.get('scope')=='original_GnmSubmitFlip_compute_census']
assert len(dispatches)==15 and all(x['dispatch_owner_status']==0 and x['dispatch_register_identity'] for x in dispatches)
startup=json.loads(Path('reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':startup['exe_sha256'],'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'row':row,'owned_dispatches':15,'R3':False,'engine_replacements':0,'limitation':'Original3615 vertex-only RectList without attachments completed with unchanged16384 viewport. EOP3622 event0x504 remains blocked on two unpublished depth owners; label/IRQ withheld. Cursor794. Ten original computes, twelve original draws and eleven color guest publications43941888B; depth/stencil guest publication remains missing. No frame/flip.'}
Path('reports/reconstruction/infra/ordered_route_handoff_retail.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(row))
