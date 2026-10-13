# SPDX-License-Identifier: GPL-2.0-only
"""Scoped host controls and original cold evidence; no game reconstruction credit."""
from pathlib import Path
import subprocess,json,hashlib,datetime
root=Path('.')
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
exe=Path('build/CTRPort-infra/ctrport.exe')
controls=[]
for name in ['thread-handle-prefix','thread-numeric-id','thread-identity-lifecycle','startup-worker-retained-lookup','worker-join-bridge','live-worker-priority','np-toolkit-dispatch','system-status']:
 c=subprocess.run([str(exe),'--'+name+'-control'],capture_output=True,text=True,timeout=15)
 assert c.returncode==0,(name,c.returncode,c.stderr)
 controls.append(dict(command='--'+name+'-control',exit_code=c.returncode,events=[json.loads(l) for l in c.stdout.splitlines() if l.startswith('{')]))
cold=Path('reports/reconstruction/infra/json2_compat_startup.json');r=json.loads(cold.read_text())
assert r['exe_sha256']==digest(exe)
scopes={}
for l in Path(r['stdout']).read_text(errors='replace').splitlines():
 if len(l)>2000 or not l.startswith('{'):continue
 try:e=json.loads(l)
 except ValueError:continue
 s=e.get('scope','')
 if s in ['np_toolkit_event_queue_checked','np_toolkit_callback_register','net_toolkit_offline_callback_dispatch','original_CondSignal_return','owned_system_status_return']:scopes.setdefault(s,[]).append(e)
for s in ['np_toolkit_event_queue_checked','owned_system_status_return']:assert scopes.get(s),s
assert not r['original_access_faults'],r['original_access_faults']
called={e['registry_index']:e['public_name'] for e in r['called_stubs']}
report=dict(timestamp=datetime.datetime.now(datetime.timezone.utc).isoformat(),scope='owned thread tid prefix, NP event queue and foreground SystemService status',passed=True,exe_sha256=digest(exe),cold_report_sha256=digest(cold),cold_exit=r['exit_code'],next_boundary=r.get('terminal_boundary'),original_access_faults=r['original_access_faults'],original_events=scopes,controls=controls,source_sha256={str(p):digest(p) for p in map(Path,['ctrport/thread_identity.hpp','ctrport/startup_worker.hpp','ctrport/system_parameters.hpp','ctrport/kernel_services.asm','ctrport/entry_boundary.hpp'])},public_reference=dict(revision='e3ce810f3a653f43ac64ebab63023de281a4103a',license='GPL-2.0-or-later',files=['src/core/libraries/kernel/threads/pthread.h','src/core/libraries/np/np_manager.cpp','src/core/libraries/system/systemservice.h','src/core/libraries/system/systemservice.cpp']),retail_failing_case=dict(image_sha256='480a769db6697a8aa3b17019f0efe60095b85f05f8bfc67f13ed3efb675e5c9c',offset=283617,operation='read int32 at pthread_self return',old_fault_address=92),active_stubs=r['active_stubs'],distinct_called_stubs=called,R3_previously_verified=True,R4=False,R5=False,engine_replacements=0,limitations=['Thread handle exposes only verified int32 tid prefix, not full Pthread layout or scheduler equivalence.','Authored NP events are control-only; offline runtime creates no PSN events; original empty-queue check observed.','System status uses owned foreground/no-overlay/no-broadcast/no-VR execution policy and current event count; additional shell event producers are not implemented.','No title/menu visual or audible audio/controller navigation verification.'])
Path('reports/reconstruction/infra/thread_np_system_status.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps(dict(passed=True,original_events={k:len(v) for k,v in scopes.items()},next_boundary=r.get('terminal_boundary'),engine_replacements=0)))
