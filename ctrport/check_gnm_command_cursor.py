# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json,subprocess
from pathlib import Path
exe=Path('build/CTRPort-infra/ctrport-host-compiler.exe')
r=subprocess.run([str(exe),'--gnm-command-cursor-control'],capture_output=True,text=True,timeout=20)
assert r.returncode==0,(r.returncode,r.stdout,r.stderr)
e=next(json.loads(x) for x in r.stdout.splitlines() if x.startswith('{'))
assert e['passed'] and e['failure_keeps_cursor'] and e['resume_without_duplicate_completion'] and e['exception_and_reentry_guarded'] and not e['GPU_execution']
report={'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'control':e,'exit':0,'R3':False,'engine_replacements':0,'limitation':'Authored handler completion tests only. Runtime has no admitted packet-effect handler; cursor stops at the first original packet without skipping it.'}
Path('reports/reconstruction/infra/gnm_command_cursor_control.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(e))
