# SPDX-License-Identifier: GPL-2.0-only
"""Audit all reached compute binary captures; no compiler/execution claim."""
import hashlib,json,collections
from pathlib import Path
root=Path(__file__).resolve().parents[1]
r=json.loads((root/'reports/reconstruction/infra/shareplay_pending_startup.json').read_text())
assert hashlib.sha256(Path(r['exe_path']).read_bytes()).hexdigest()==r['exe_sha256']
e=[x for x in r['events'] if x.get('scope')=='original_GnmSubmitFlip_compute_census']
canonical=[]
for line in (root/'runtime/user/logs/submit_flip_census.jsonl').read_text(errors='replace').splitlines():
 if line.startswith('{'):
  try:item=json.loads(line)
  except ValueError:continue
  if item.get('scope')=='original_GnmSubmitFlip_compute_census':canonical.append(item)
assert e==canonical
ordered=next(x for x in r['events'] if isinstance(x.get('packets'),list))
packets=[(w,n) for w,n,op in ordered['packets'] if op==0x15]
assert len(e)==15 and [x['word'] for x in e]==[w for w,n in packets]
assert all(n==5 for w,n in packets)
routes=[x for x in ordered['execution_routes'] if x['opcode']==0x15]
for x,route in zip(e,routes):
 assert x['word']==route['word'] and x['code_address']==route['compute_code']
 assert x['state_status']==x['lease_status']==x['capture_status']==x['lease_end_status']==0
 assert x['dispatch_owner_status']==0 and x['dispatch_register_identity']
 assert x['range_begins']==x['range_ends'] and x['active_leases']==x['active_snapshots']==0
 assert 0<x['validated_code_bytes']<=x['footer_offset'] and x['validated_code_bytes']%4==0
 assert x['footer_offset']+28<=x['capture_extent']<=65564
 assert len(x['workgroup'])==len(x['partial'])==len(x['dispatch_groups'])==3
 assert all(0<=v<=65535 for v in x['workgroup']+x['partial'])
 assert 0<=x['user_registers']<=16 and x['shared_bytes']%512==0
 assert not x['source_writer_coherence'] and not x['compiler_admission'] and not x['gpu_execution']
programs=collections.Counter((x['code_address'],x['validated_code_bytes']) for x in e)
assert len(programs)==2
out={'scope':'retail_whole_compute_binary_census','eboot_sha256':json.loads((root/'reports/reconstruction/infra/first_draw_shader_capture.json').read_text())['eboot_sha256'],'exe_sha256':r['exe_sha256'],'dispatches':len(e),'observed_code_address_and_size_groups':[{'address':a,'code_bytes':b,'dispatches':n} for (a,b),n in programs.items()],'events':e,'active_stubs':r['active_stubs'],'called_stubs':r['called_stubs'],'R3':False,'engine_replacements':0,'payload_persisted':False,'limitation':'Owned binary copies and retained compute/dispatch metadata only. No shader compilation, resources, writable output, guest-writer coherence, GPU dispatch or flip. Two observed code addresses are not a content-equivalence assertion. Pending SharePlay never returns.','public_source':{'revision':'e3ce810f3a653f43ac64ebab63023de281a4103a','path':'engine/shadPS4/src/video_core/amdgpu/pm4_cmds.h','license':'GPL-2.0-or-later'},'repeat':'CTR_GRAPHICS_CENSUS_STAGES=84 python -B ctrport/check_pending_startup.py; python -B ctrport/audit_compute_census.py'}
(root/'reports/reconstruction/infra/compute_submission_binary_census.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({k:v for k,v in out.items() if k not in ('events','called_stubs')}))
