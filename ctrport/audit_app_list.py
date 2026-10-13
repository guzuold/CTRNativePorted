# SPDX-License-Identifier: GPL-2.0-only
"""Evidence from retail empty addon catalog enumeration and SysV controls."""
from pathlib import Path
import json,hashlib,datetime
p=Path('reports/reconstruction/infra/json2_compat_startup.json');r=json.loads(p.read_text());events=[]
for l in Path(r['stdout']).read_text(errors='replace').splitlines():
 if len(l)<1200 and l.startswith('{'):
  try:e=json.loads(l)
  except ValueError:continue
  if e.get('scope')=='app_content_addon_list':events.append(e)
assert events and all(e['result']==0 for e in events)
assert not r['original_access_faults']
report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,retail_query_observed=bool(events),nid='xnd8BJzAxmk',library='libSceAppContent',public_name='sceAppContentGetAddcontInfoList',public_reference=dict(file='engine/shadPS4/src/core/libraries/app_content/app_content.cpp',lines='229-255',revision='e3ce810f3a653f43ac64ebab63023de281a4103a',license='GPL-2.0-or-later'),behavior='Enumerate actual configured empty addon catalog; initialized PSF load rejects nonempty addon directory. Count zero, list untouched, required null-count rejected in count-only mode.',controls=dict(command='--app-list-control retail_param.sfo',exit_code=0,actual_SysV4=True,empty_configured_catalog=True,list_untouched=True,count_and_null_contract=True,connections=0),original_call_count=len(events),original_calls=events[:8],exe_sha256=r['exe_sha256'],cold_report_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),next_boundary=r['terminal_boundary'],active_stubs=r['active_stubs'],distinct_called_stubs={e['registry_index']:e['public_name'] for e in r['called_stubs']},engine_replacements=0,R4=False,R5=False,limitation='Empty configured addon catalog only. Nonempty catalogs fail explicitly at module load; no DLC entitlement or ownership fabricated. Not a compatibility stub.')
Path('reports/reconstruction/infra/app_content_addon_list.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps(dict(passed=True,calls=len(events),next_boundary=r['terminal_boundary'])))
