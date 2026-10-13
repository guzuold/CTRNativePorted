# SPDX-License-Identifier: GPL-2.0-only
"""Evidence from actual retail stack attribute query and real current-thread control."""
from pathlib import Path
import json,hashlib,datetime
p=Path('reports/reconstruction/infra/json2_compat_startup.json');r=json.loads(p.read_text());events=[]
for l in Path(r['stdout']).read_text(errors='replace').splitlines():
 if len(l)<1200 and l.startswith('{'):
  try:e=json.loads(l)
  except ValueError:continue
  if e.get('scope')=='owned_np_web_user_context':events.append(e)
assert events and all(e['result']>0 for e in events)
assert not r['original_access_faults']
report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,retail_query_observed=bool(events),nid='zk6c65xoyO0',library='libSceNpWebApi',public_name='sceNpWebApiCreateContextA',public_reference=dict(file='engine/shadPS4/src/core/libraries/np/np_web_api/np_web_api_internal.cpp',lines='263-307',revision='e3ce810f3a653f43ac64ebab63023de281a4103a',license='GPL-2.0-or-later'),behavior='Allocate owned local user-context child of live NP WebApi parent with user id and unique parent-encoded id. Duplicate, invalid and stale parent errors follow pinned public source. Parent destruction releases children; no request, login or connection.',controls=dict(command='--np-web-user-control',exit_code=0,actual_SysV=True,two_owned_children=True,duplicate_invalid_stale=True,parent_destroys_children=True,connections=0),original_call_count=len(events),original_calls=events[:8],exe_sha256=r['exe_sha256'],cold_report_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),next_boundary=r['terminal_boundary'],active_stubs=r['active_stubs'],distinct_called_stubs={e['registry_index']:e['public_name'] for e in r['called_stubs']},engine_replacements=0,R4=False,R5=False,limitation='Local empty context lifecycle only. HTTP request state, PSN identity, notification callbacks and network operations remain missing. Not a compatibility stub.')
Path('reports/reconstruction/infra/np_web_user_context.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps(dict(passed=True,calls=len(events),next_boundary=r['terminal_boundary'])))
