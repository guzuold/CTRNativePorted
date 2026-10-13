# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('runtime/user/logs/submit_flip_census.jsonl')
 rows=[json.loads(line) for line in source.read_text().splitlines()]
 def one(scope):
  result=[r for r in rows if r.get('scope')=='original_GnmSubmitFlip_'+scope and r.get('word')==1377]
  assert len(result)==1,(scope,len(result))
  return result[0]
 key=one('graphics_key');coherence=one('ordered_source_coherence');pipeline=one('first_pipeline');pending=one('MRT_GPU_draw')
 assert key['status']==0 and key['samples']==key['depth_samples']==2 and key['mrt_mask']==3 and key['color_count']==2 and key['z_format']==3
 assert not key['mixed_depth_enabled'] and not key['mixed_any_enabled'] and coherence['status']==0
 copies=[r for r in rows if r.get('scope')=='original_GnmSubmitFlip_color_MS_backing_copy' and r.get('word')==1377]
 assert len(copies)==2 and [r['slot'] for r in copies]==[0,1]
 assert all(r['status']==0 and r['direction']==0 and r['GPU_execution'] and r['original_key_unchanged'] and not r['guest_writeback'] for r in copies)
 assert pipeline['status']==0 and pipeline['owned_token'] and not pipeline['GPU_execution']
 roundtrips=[r for r in rows if r.get('scope')=='original_GnmSubmitFlip_color_MS_sample0_roundtrip' and r.get('word')==1377]
 assert [r['slot'] for r in roundtrips]==[0,1] and [r['compared_bytes'] for r in roundtrips]==[8355840,4423680]
 assert all(r['status']==0 and r['differences']==0 and r['GPU_execution'] and not r['averaged_resolve'] and not r['pixel_payload_persisted'] for r in roundtrips)
 assert pending['status']==0 and pending['invoked'] and pending['GPU_execution'] and pending['completed_packets']==317
 assert rows.index(coherence)<rows.index(copies[0])<rows.index(copies[1])<rows.index(pipeline)<rows.index(pending)
 report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,exe_sha256=hashlib.sha256(Path('build/CTRPort-infra/ctrport.exe').read_bytes()).hexdigest(),census_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),key=key,source_coherence=coherence,copies=copies,sample0_roundtrips=roundtrips,pipeline=pipeline,pending=pending,R3=False,engine_replacements=0,limitation='Actual original1377 shaders and unchanged public fallback key; both current color generations copied to real MS2 backing and sample0 returned with full byte comparison. Combined original1377 depth/stencil draw now completed; full post-draw sample0 readbacks recorded in mrt_depth_draw_retail.json. No guest tiled writeback yet.')
 Path('reports/reconstruction/infra/mrt_backing_pipeline_retail.json').write_text(json.dumps(report,indent=2)+'\n')
 print('original1377 MS backing / combined pipeline audit passed')
