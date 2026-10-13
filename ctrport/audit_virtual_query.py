# SPDX-License-Identifier: GPL-2.0-only
"""Retain actual direct owner query and scoped SysV control evidence."""
from pathlib import Path
import json,hashlib,datetime
p=Path('reports/reconstruction/infra/json2_compat_startup.json');r=json.loads(p.read_text());events=[]
for line in Path(r['stdout']).read_text(errors='replace').splitlines():
 if len(line)<1400 and line.startswith('{'):
  try:e=json.loads(line)
  except ValueError:continue
  if e.get('scope')=='original_VirtualQuery_return':events.append(e)
control=[json.loads(l) for l in Path('runtime/user/logs/virtual_query_control.log').read_text().splitlines() if l.startswith('{')]
assert any(e.get('scope')=='virtual_query_control' and e.get('passed') for e in control)
assert events and all(e['result']==0 and e['start']<=e['address']<e['end'] for e in events)
heap=[l for l in Path(r['stderr']).read_text(errors='replace').splitlines() if 'code=0xc0000374' in l]
report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=not heap and not r['original_access_faults'],nid='rVjRvHJ0X6c',library='libkernel',public_name='sceKernelVirtualQuery',public_reference=dict(revision='e3ce810f3a653f43ac64ebab63023de281a4103a',license='GPL-2.0-or-later',files=['src/core/memory.cpp:1138-1183','src/core/libraries/kernel/memory.h:61-75']),behavior='Actual owned direct mapping ledger, protection splits updated transactionally with TypeProtect, physical offset and allocation type; layout72. No host-page authority.',control=control,original_calls=events,exe_sha256=r['exe_sha256'],cold_report_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),next_boundary=r['terminal_boundary'],service_diagnostics=r['service_diagnostics'],active_stubs=r['active_stubs'],distinct_called_stubs={e['registry_index']:e['public_name'] for e in r['called_stubs']},engine_replacements=0,R4=False,R5=False,heap_corruption=heap,limitation='Only flags0/size72 and owned direct mappings; other flags/owners explicitly fail. First cold heap corruption before query retained separately; cause unresolved, subsequent run cannot prove repaired. No new stub, no full boot/menu/audio credit.')
Path('reports/reconstruction/infra/virtual_query_owned_mapping.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps(dict(passed=report['passed'],query_calls=len(events),next_boundary=r['terminal_boundary'])))
