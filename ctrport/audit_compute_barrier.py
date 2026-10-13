# SPDX-License-Identifier: GPL-2.0-only
import json,hashlib,datetime
from pathlib import Path
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 source=Path('runtime/user/logs/submit_flip_census.jsonl');rows=list(map(json.loads,source.read_text().splitlines()))
 def one(name):
  result=[r for r in rows if r.get('scope')=='original_GnmSubmitFlip_'+name and (name!='acquire_mem_GPU_visibility' or r.get('word')==690)];assert len(result)==1;return result[0]
 barrier=one('acquire_mem_GPU_visibility');compute=one('first_compute_compiled');pending=one('ordered_route_handoff')
 assert barrier['word']==690 and barrier['status']==0 and barrier['real_fence'] and barrier['queue_wide_memory_dependency'] and not barrier['guest_writeback']
 assert compute['word']==755 and compute['status']==0 and compute['spirv_words']==871 and compute['descriptor_count']==2 and compute['resources_buffers']==compute['resources_images']==1 and compute['owned_after_DLL_unload'] and not compute['GPU_execution']
 assert pending['word']>983 and pending['completed_packets']>=231 and pending['status']==88
 report=json.loads(Path('reports/reconstruction/infra/compute_barrier_retail.json').read_text())
 assert report['census_sha256']==hashlib.sha256(source.read_bytes()).hexdigest() and report['barrier']==barrier and report['compute']==compute and report['pending']==pending
 assert report['exe_sha256']==hashlib.sha256(Path('build/CTRPort-infra/ctrport.exe').read_bytes()).hexdigest()
 print('compute/barrier retail audit passed')
