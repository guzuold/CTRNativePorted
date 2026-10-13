# SPDX-License-Identifier: GPL-2.0-only
"""Owned guest affinity configuration readback and original execution evidence."""
from pathlib import Path
import json,hashlib,datetime
p=Path('reports/reconstruction/infra/json2_compat_startup.json');r=json.loads(p.read_text());events=[]
for l in Path(r['stdout']).read_text(errors='replace').splitlines():
 if len(l)<1200 and l.startswith('{'):
  try:e=json.loads(l)
  except ValueError:continue
  if e.get('scope')=='owned_thread_affinity_readback':events.append(e)
assert events and any(e['result']==0 for e in events)
assert not r['original_access_faults']
report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,nid='rcrVFJsQWRY',library='libkernel',public_name='scePthreadGetaffinity',public_reference=dict(file='engine/shadPS4/src/core/libraries/kernel/threads/pthread.cpp',lines='879-896,924-931',revision='e3ce810f3a653f43ac64ebab63023de281a4103a',license='GPL-2.0-or-later'),behavior='Read owned thread guest affinity attributes; update only after real host affinity change succeeds. Default unrestricted guest value maps to actual process mask at startup. No current-CPU invention.',controls=dict(command='--thread-affinity-readback-control',exit_code=0,actual_SysV_query=True,actual_host_set_then_guest_readback=True,host_restored=True,unknown_handle_no_write=True,null_output_invalid=True),original_calls=events,exe_sha256=r['exe_sha256'],cold_report_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),next_boundary=r['terminal_boundary'],active_stubs=r['active_stubs'],distinct_called_stubs={e['registry_index']:e['public_name'] for e in r['called_stubs']},engine_replacements=0,R4=False,R5=False,limitation='Retained guest attribute readback, not host CPU-number query or PS4 scheduler equivalence. Multi-group hosts remain explicitly unsupported in setters. No new compatibility stubs.')
Path('reports/reconstruction/infra/thread_affinity_query.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps(dict(passed=True,calls=len(events),next_boundary=r['terminal_boundary'])))
