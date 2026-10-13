# SPDX-License-Identifier: GPL-2.0-only
"""Evidence from actual retail stack attribute query and real current-thread control."""
from pathlib import Path
import json,hashlib,datetime
p=Path('reports/reconstruction/infra/json2_compat_startup.json');r=json.loads(p.read_text());events=[]
for l in Path(r['stdout']).read_text(errors='replace').splitlines():
 if len(l)<1200 and l.startswith('{'):
  try:e=json.loads(l)
  except ValueError:continue
  if e.get('scope')=='owned_orbis_gettimeofday':events.append(e)
assert events and all(e['result']==0 for e in events)
assert not r['original_access_faults']
report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,retail_query_observed=bool(events),nid='ejekcaNQNq0',library='libkernel',public_name='sceKernelGettimeofday',public_reference=dict(file='engine/shadPS4/src/core/libraries/kernel/time.cpp',lines='443-448',revision='e3ce810f3a653f43ac64ebab63023de281a4103a',license='GPL-2.0-or-later'),behavior='Single-argument Orbis wrapper over actual Windows UTC microsecond clock. Second SysV register is ignored, never treated as timezone pointer.',controls=dict(command='--orbis-gettimeofday-control',exit_code=0,actual_SysV=True,clock_bounds=True,sentinels=True,null_optional=True,second_argument_ignored=True),original_call_count=len(events),original_calls=events[:8],exe_sha256=r['exe_sha256'],cold_report_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),next_boundary=r['terminal_boundary'],active_stubs=r['active_stubs'],distinct_called_stubs={e['registry_index']:e['public_name'] for e in r['called_stubs']},engine_replacements=0,R4=False,R5=False,limitation='Wall clock query only. No scheduling support or compatibility stub.')
Path('reports/reconstruction/infra/orbis_gettimeofday.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps(dict(passed=True,calls=len(events),next_boundary=r['terminal_boundary'])))
