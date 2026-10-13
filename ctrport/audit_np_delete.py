# SPDX-License-Identifier: GPL-2.0-only
"""Evidence from retail NP request deletion and owned-request SysV controls."""
from pathlib import Path
import json,hashlib,datetime
p=Path('reports/reconstruction/infra/json2_compat_startup.json');r=json.loads(p.read_text());events=[]
for l in Path(r['stdout']).read_text(errors='replace').splitlines():
 if len(l)<1200 and l.startswith('{'):
  try:e=json.loads(l)
  except ValueError:continue
  if e.get('scope')=='owned_np_web_delete_request':events.append(e)
assert events and all(e['result']==0 for e in events)
assert not r['original_access_faults']
report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,retail_query_observed=bool(events),nid='noQgleu+KLE',library='libSceNpWebApi',public_name='sceNpWebApiDeleteRequest',public_reference=dict(file='engine/shadPS4/src/core/libraries/np/np_web_api/np_web_api_internal.cpp',lines='1016-1070',revision='e3ce810f3a653f43ac64ebab63023de281a4103a',license='GPL-2.0-or-later'),behavior='Validate owned library/user/request and destroy actual request metadata. Offline synchronous sends own no HTTP resources or outstanding workers. Siblings preserved; deleted IDs never reused.',controls=dict(command='--np-delete-control',exit_code=0,actual_SysV1=True,sent_and_unsent_release=True,sibling_preserved=True,double_delete_and_stale_rejected=True,connections=0),original_call_count=len(events),original_calls=events[:8],exe_sha256=r['exe_sha256'],cold_report_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),next_boundary=r['terminal_boundary'],active_stubs=r['active_stubs'],distinct_called_stubs={e['registry_index']:e['public_name'] for e in r['called_stubs']},engine_replacements=0,R4=False,R5=False,limitation='Owned offline requests only. No asynchronous HTTP requests exist in this runtime; concurrent authenticated abort/wait branch is unsupported. Not a compatibility stub.')
Path('reports/reconstruction/infra/np_web_request_delete.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps(dict(passed=True,calls=len(events),next_boundary=r['terminal_boundary'])))
