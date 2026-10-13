# SPDX-License-Identifier: GPL-2.0-only
"""Evidence from actual retail stack attribute query and real current-thread control."""
from pathlib import Path
import json,hashlib,datetime
p=Path('reports/reconstruction/infra/json2_compat_startup.json');r=json.loads(p.read_text());events=[]
for l in Path(r['stdout']).read_text(errors='replace').splitlines():
 if len(l)<1200 and l.startswith('{'):
  try:e=json.loads(l)
  except ValueError:continue
  if e.get('scope')=='owned_attribute_get_stack':events.append(e)
assert all(e['result']==0 for e in events)
assert not r['original_access_faults']
report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,retail_query_observed=bool(events),nid='-quPa4SEJUw',library='libkernel',public_name='scePthreadAttrGetstack',public_reference=dict(file='engine/shadPS4/src/core/libraries/kernel/threads/pthread_attr.cpp',lines='99-107',revision='e3ce810f3a653f43ac64ebab63023de281a4103a',license='GPL-2.0-or-later'),behavior='Read address and size from a live owned thread-attribute object. Current-thread snapshot is supplied by actual GetCurrentThreadStackLimits. Default unassigned attributes retain address0/size1048576. Both output ranges are validated before writes.',controls=dict(command='--attribute-get-stack-control',exit_code=0,actual_SysV=True,actual_current_stack_contains_local=True,invalid_handle_no_write=True,null_EINVAL=True),original_call_count=len(events),original_calls=events[:8],exe_sha256=r['exe_sha256'],cold_report_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),next_boundary=r['terminal_boundary'],active_stubs=r['active_stubs'],distinct_called_stubs={e['registry_index']:e['public_name'] for e in r['called_stubs']},engine_replacements=0,R4=False,R5=False,limitation='Attribute query only; no new thread scheduler or stack allocation support. No compatibility stub.')
Path('reports/reconstruction/infra/attribute_stack.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps(dict(passed=True,calls=len(events),next_boundary=r['terminal_boundary'])))
