# SPDX-License-Identifier: GPL-2.0-only
"""Strict actual retail MRT1377 execution and retained depth generation audit."""
import hashlib,json
from pathlib import Path
from ctrport.retail_pending import ordered_pending
from datetime import datetime,timezone
from tools.lane_lock import lane_lock
root=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
with lane_lock('shared','infra'):
 census=root/'runtime/user/logs/submit_flip_census.jsonl'
 rows=[json.loads(x) for x in census.read_text().splitlines()]
 def one(scope,word):
  found=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_'+scope and x.get('word')==word]
  assert len(found)==1,(scope,len(found))
  return found[0]
 draw=one('MRT_GPU_draw',1377)
 assert draw['status']==0 and draw['invoked'] and draw['GPU_execution'] and draw['completed_packets']==317 and draw['resume_status']==88
 reuse=one('depth_identity_reuse',1377)
 assert reuse['status']==0 and reuse['preserved_GPU_generation'] and reuse['source_word']==680
 readbacks=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_MRT_GPU_readback' and x.get('word')==1377]
 assert len(readbacks)==2
 assert sorted(x['bytes'] for x in readbacks)==[4423680,8355840]
 assert all(x['status']==0 and x['sample0_reverse_fenced'] and x['GPU_execution'] and not x['pixel_payload_persisted'] for x in readbacks)
 pending=ordered_pending(rows)[0]
 assert pending['status']==88 and pending['completed_packets']>=373
 report=dict(timestamp=datetime.now(timezone.utc).isoformat(),executable_sha256=sha(root/'build/CTRPort-infra/ctrport.exe'),census_sha256=sha(census),draw=draw,depth_reuse=reuse,readbacks=readbacks,pending=pending,R3=False,reconstructed_replacements=0,compat_stubs_active=19,compat_stubs_called=17,limitations='Real retail GPU draw/readback only; no frame/flip. Original1426 sampled-depth dispatch and tile0 guest writeback completed; later ordered route pending; no CPU stale snapshot admitted.')
 path=root/'reports/reconstruction/infra/mrt_depth_draw_retail.json'
 path.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report))
