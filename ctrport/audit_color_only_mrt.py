# SPDX-License-Identifier: GPL-2.0-only
import json,hashlib,datetime
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('runtime/user/logs/submit_flip_census.jsonl');rows=[json.loads(x) for x in source.read_text().splitlines()]
 pick=lambda scope:[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_'+scope and x.get('word')==2336]
 init=pick('color_initialization');assert len(init)==2 and all(x['upload_status']==x['readback_status']==0 and x['readback_pixel_bytes']==8355840 for x in init)
 sources=pick('sampled_source_admission');assert {x['sharp'] for x in sources}=={0,16,24} and all(x['status']==0 for x in sources)
 pipe=pick('first_pipeline');assert pipe and all(x['status']==0 and x['owned_token'] for x in pipe)
 draws=pick('MRT_GPU_draw');assert len(draws)==1 and draws[0]['status']==0 and draws[0]['invoked'] and draws[0]['GPU_execution'] and draws[0]['completed_packets']>=561
 reads=pick('MRT_GPU_readback');assert len(reads)==2 and all(x['status']==0 and x['bytes']==8355840 and not x['sample0_reverse_fenced'] for x in reads)
 writes=pick('color_guest_writeback');assert len(writes)==2 and all(x['status']==0 and x['guest_bytes']==8355840 and x['actual_GPU_retiled_generation'] and not x['poisoned'] for x in writes)
 report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,initialization=init,sources=sources,pipeline=pipe,draws=draws,readbacks=reads,writes=writes,exe_sha256=hashlib.sha256(Path('build/CTRPort-infra/ctrport.exe').read_bytes()).hexdigest(),census_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),R3=False,engine_replacements=0,scope='Original2336 shaders/key execute genuine single-sample R32float/RGBA8 two-color draw. Real fenced full output readbacks/public GPU retile/guest reread; no golden shader-output equivalence claim. Retained depth sampled from original680/1377, no CPU upload. No frame/flip; depth/stencil final guest publication missing.')
 Path('reports/reconstruction/infra/color_only_mrt_retail.json').write_text(json.dumps(report,indent=2)+'\n');print('original2336 color-only MRT GPU and two guest publications passed')
