# SPDX-License-Identifier: GPL-2.0-only
"""Actual retail retained depth alias preparation and exit restoration evidence."""
import json,hashlib,datetime
from pathlib import Path
from ctrport.retail_pending import ordered_pending
from tools.lane_lock import lane_lock

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
with lane_lock('shared','infra'):
 source=Path('runtime/user/logs/submit_flip_census.jsonl');rows=[json.loads(x) for x in source.read_text().splitlines()]
 def one(name,word):
  events=[x for x in rows if x.get('scope')=='original_GnmSubmitFlip_'+name and x.get('word')==word]
  assert len(events)==1,(name,events)
  return events[0]
 reuse=one('depth_identity_reuse',1377);assert reuse['status']==0 and reuse['source_word']==680 and reuse['preserved_GPU_generation']
 alias=one('compute_depth_sampled_alias',1426)
 assert alias['status']==0 and alias['source_word']==680 and alias['same_retained_GPU_owner'] and alias['owned_token'] and not alias['CPU_snapshot_uploaded']
 restore=one('compute_depth_restore',1426);assert restore['status']==0 and restore['real_fence']
 dispatch=one('compute_GPU_dispatch',1426);assert dispatch['status']==dispatch['guest_writeback_status']==0 and dispatch['guest_bytes']==9437184 and dispatch['writeback_completed'] and dispatch['GPU_execution']
 sampled=one('compute_sampled_initialized',1426);assert sampled['status']==0 and sampled['retained_depth_alias'] and not sampled['CPU_snapshot_rechecked'] and sampled['bytes']==0
 pending=ordered_pending(rows);assert len(pending)==1 and pending[0]['status']==88 and pending[0]['completed_packets']>=373
 report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,executable_sha256=sha(Path('build/CTRPort-infra/ctrport.exe')),source_sha256=sha(source),retained_depth_reuse=reuse,alias=alias,restore=restore,dispatch=dispatch,sampled=sampled,pending=pending[0],R3=False,engine_replacements=0,scope='Actual original680/1377 GPU depth owner sampled alias creation and fenced restoration. Original1426 dispatch and full9437184B guest writeback passed; no independent shader-output equivalence or frame.')
 Path('reports/reconstruction/infra/depth_sampled_alias_retail.json').write_text(json.dumps(report,indent=2)+'\n');print('retail retained depth alias and restore passed; original1426 dispatch/writeback passed')
