# SPDX-License-Identifier: GPL-2.0-only
"""Observed offline NP state-queue check, not online service equivalence."""
from pathlib import Path
import json,hashlib,datetime
p=Path('reports/reconstruction/infra/json2_compat_startup.json');r=json.loads(p.read_text());events=[]
for l in Path(r['stdout']).read_text(errors='replace').splitlines():
 if len(l)<1200 and l.startswith('{'):
  try:e=json.loads(l)
  except ValueError:continue
  if e.get('scope')=='np_general_event_queue_checked':events.append(e)
assert events and all(e['result']==0 and e['events_consumed']==0 for e in events)
assert not r['original_access_faults']
report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,nid='3Zl8BePTh9Y',library='libSceNpManager',module='libSceNpManager',public_name='sceNpCheckCallback',public_reference=dict(file='engine/shadPS4/src/core/libraries/np/np_manager.cpp',lines='903-973',revision='e3ce810f3a653f43ac64ebab63023de281a4103a',license='GPL-2.0-or-later'),domain='Same owned pending-state queue as toolkit; async producers and other state callback registrations remain missing and cannot create pending jobs.',controls=dict(np_general_check='actual SysV empty queue passed exit0',shared_np_toolkit_dispatch='authored2 events actualSysV3 consumed once passed exit0',scope='synthetic host control; original cold empty queue observed below'),original_calls=events,exe_sha256=r['exe_sha256'],cold_report_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),next_boundary=r['terminal_boundary'],active_stubs=r['active_stubs'],distinct_called_stubs={e['registry_index']:e['public_name'] for e in r['called_stubs']},engine_replacements=0,R4=False,R5=False,limitation='No PSN events, identities, async completion or online equivalence fabricated. No new compatibility stubs. Audio submission is not audible verification.')
Path('reports/reconstruction/infra/np_general_check.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps(dict(passed=True,calls=len(events),next_boundary=r['terminal_boundary'])))
