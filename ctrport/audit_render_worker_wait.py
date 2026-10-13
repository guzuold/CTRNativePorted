# SPDX-License-Identifier: GPL-2.0-only
"""Correlate real semaphore calls with a bounded native thread checkpoint."""
import json,hashlib,datetime,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent.parent))
from tools.lane_lock import lane_lock
with lane_lock('shared','infra'):
 p=Path('reports/reconstruction/infra/shareplay_pending_startup.json');r=json.loads(p.read_text());events=r['events']
 assert r['exit']=='diagnostic_timeout_killed' and r['timeout_seconds']==120 and r['thread_checkpoint_status']==0
 assert hashlib.sha256(Path(r['exe_path']).read_bytes()).hexdigest()==r['exe_sha256']
 flips=[e for e in events if e.get('scope')=='original_GnmInsertWaitFlipDone_return'];assert len(flips)==1;thread=flips[0]['thread']
 rows=[json.loads(l) for l in Path('runtime/user/logs/infra_thread_checkpoint.jsonl').read_text().splitlines()]
 summary=rows[-1];assert summary['captured']>0 and summary['rejected']==0
 checkpoint=next(e for e in rows if e.get('thread')==thread);assert checkpoint['resumed'] and not checkpoint['stack_payload_persisted'] and not checkpoint['unwind_verified']
 base=None
 for i,e in enumerate(events):
  if e.get('scope')=='original_startup_libc_dependency_boundary' and e.get('library')=='libSceSharePlay':
   before=events[i-1];assert before['scope']=='original_caller_location' and before['image_sha256']==e['source_image_sha256'];base=e['caller_address']-before['offset'];break
 assert base
 calls=[e for e in events if e.get('scope')=='original_Semaphore_call' and e['thread']==thread]
 candidates=[]
 for call in calls:
  if call['operation']!=1 or call['caller']-base not in checkpoint['return_candidates']:continue
  group=[e for e in calls if e['id']==call['id'] and e['caller']==call['caller']]
  enter=sum(e['operation']==1 for e in group);returned=sum(e['operation']==2 for e in group)
  if enter and returned and any(f"semaphore blocking id={call['id']} " in line and f'thread={thread}' in line for line in Path('runtime/user/logs/shareplay_park.stderr.log').read_text().splitlines()):
   item=dict(id=call['id'],caller_offset=call['caller']-base,thread=thread,entered=enter,returned=returned,trace_saturated=(enter==16 or returned==16))
   if item not in candidates:candidates.append(item)
 assert candidates
 control=json.loads(Path('runtime/user/logs/infra_semaphore_trace_control.log').read_text());assert control['passed'] and control['emissions_per_key']==16
 assert len([e for e in events if e.get('scope')=='original_GnmSubmitFlip_arguments'])==1
 result=dict(scope='retail_render_worker_wait_after_first_flip',passed=True,timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),exe_sha256=r['exe_sha256'],source_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),checkpoint_sha256=hashlib.sha256(Path('runtime/user/logs/infra_thread_checkpoint.jsonl').read_bytes()).hexdigest(),helper_sha256=hashlib.sha256(Path('build/CTRPort-infra/thread_checkpoint.exe').read_bytes()).hexdigest(),render_thread=checkpoint,wait_calls=candidates,checkpoint_summary=summary,trace_control=control,original_eboot_sha256='ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1',R3=False,engine_replacements=0,limitation='Return candidates are not a verified unwind. Trace is saturated and cannot establish final enter/return balance; native sample and actual blocking log identify the matched wait path; no second submission/cursor exists. Producer completion identity is established separately in producer_condition_frames.json; this semaphore audit alone makes no causal claim. No semaphore tokens, guest pointers, or query outputs were fabricated; all captured threads resumed.')
 Path('reports/reconstruction/infra/render_worker_wait.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
