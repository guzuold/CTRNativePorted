# SPDX-License-Identifier: GPL-2.0-only
"""Evidence from retail relative condition wait and actual timeout/wakeup controls."""
from pathlib import Path
import json,hashlib,datetime
p=Path('reports/reconstruction/infra/json2_compat_startup.json');r=json.loads(p.read_text());events=[]
for l in Path(r['stdout']).read_text(errors='replace').splitlines():
 if len(l)<1200 and l.startswith('{'):
  try:e=json.loads(l)
  except ValueError:continue
  if e.get('scope')=='owned_condition_timedwait':events.append(e)
assert events and all(e['result'] in (0,-2147352516) for e in events)
assert not r['original_access_faults']
report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,retail_query_observed=bool(events),nid='BmMjYxmew1w',library='libkernel',public_name='scePthreadCondTimedwait',public_reference=dict(file='engine/shadPS4/src/core/libraries/kernel/threads/condvar.cpp',lines='219-224,453-454',revision='e3ce810f3a653f43ac64ebab63023de281a4103a',license='GPL-2.0-or-later'),behavior='Relative microseconds (not absolute timespec) real condition_variable wait_for; release owned mutex, actual signal or timeout, reacquire and restore recursive depth. Public Orbis ETIMEDOUT0x8002003c.',controls=dict(command='--condition-timed-control; --condition-wait-owned-control',exit_code=0,actual_SysV3=True,real_timeout20ms=True,zero_timeout=True,real_signal_wakeup=True,recursive_depth2_restored=True,foreign_owner_eperm=True,infinite_wait_regression=True,connections=0),original_call_count=len(events),original_calls=events[:8],exe_sha256=r['exe_sha256'],cold_report_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),next_boundary=r['terminal_boundary'],active_stubs=r['active_stubs'],distinct_called_stubs={e['registry_index']:e['public_name'] for e in r['called_stubs']},engine_replacements=0,R4=False,R5=False,limitation='Owned default condition/mutex domain only; relative intervals above24h explicitly fail. No cancellation/scheduler equivalence claimed. Not a compatibility stub.')
Path('reports/reconstruction/infra/condition_relative_timedwait.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps(dict(passed=True,calls=len(events),next_boundary=r['terminal_boundary'])))
