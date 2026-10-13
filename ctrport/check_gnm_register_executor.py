# SPDX-License-Identifier: GPL-2.0-only
import datetime,hashlib,json,subprocess,os
from pathlib import Path
exe=Path(os.environ.get('CTR_PENDING_EXECUTABLE','build/CTRPort-infra/ctrport-host-compiler.exe'))
r=subprocess.run([str(exe),'--gnm-register-executor-control'],capture_output=True,text=True,timeout=20)
assert r.returncode==0,(r.returncode,r.stdout,r.stderr)
e=next(json.loads(x) for x in r.stdout.splitlines() if x.startswith('{'))
assert e['passed'] and e['invalid_atomic'] and e['predicate_and_barrier_pending']
Path('reports/reconstruction/infra/gnm_register_executor_control.json').write_text(json.dumps({'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'control':e,'R3':False,'engine_replacements':0,'limitation':'Software register-file effects only, no GPU command completion or draw.'},indent=2)+'\n')
print(json.dumps(e))
