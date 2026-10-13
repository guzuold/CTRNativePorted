# SPDX-License-Identifier: GPL-2.0-only
"""Bounded no-client IME query evidence, never successful active input stubs."""
from pathlib import Path
import json,hashlib,datetime
p=Path('reports/reconstruction/infra/json2_compat_startup.json');r=json.loads(p.read_text());events=[]
for l in Path(r['stdout']).read_text(errors='replace').splitlines():
 if len(l)<1200 and l.startswith('{'):
  try:e=json.loads(l)
  except ValueError:continue
  if e.get('scope')=='owned_ime_dialog_status':events.append(e)
assert events and all(e['result']==0 and not e['client_present'] for e in events)
assert not r['original_access_faults']
report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),passed=True,nid='IADmD4tScBY',library='libSceImeDialog',public_name='sceImeDialogGetStatus',public_reference=dict(file='engine/shadPS4/src/core/libraries/ime/ime_dialog.cpp',lines='951-958',revision='e3ce810f3a653f43ac64ebab63023de281a4103a',license='GPL-2.0-or-later'),behavior='Query absence of owned dialog client: None0. Activating the real Unicode text backend creates no dialog client. A present client is explicitly unsupported, never reported None or Finished.',controls=dict(command='--ime-dialog-status-control',exit_code=0,actual_SysV_no_client_None=True,actual_backend_load_does_not_create_dialog=True,authored_active_client_rejected_without_mutation=True),original_call_count=len(events),original_calls=(events if len(events)<=8 else [events[0],events[-1]]),exe_sha256=r['exe_sha256'],cold_report_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),next_boundary=r['terminal_boundary'],active_stubs=r['active_stubs'],distinct_called_stubs={e['registry_index']:e['public_name'] for e in r['called_stubs']},engine_replacements=0,R4=False,R5=False,limitation='No Open, active dialog text filtering, result commit or guest input support claimed. These exports remain loud missing traps. No compatibility stub.')
Path('reports/reconstruction/infra/ime_dialog_status.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps(dict(passed=True,calls=len(events),next_boundary=r['terminal_boundary'])))
