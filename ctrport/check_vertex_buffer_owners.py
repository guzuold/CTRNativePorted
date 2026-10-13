# SPDX-License-Identifier: GPL-2.0-only
import subprocess,json,hashlib,datetime
from pathlib import Path
exe=Path('build/CTRPort-infra/ctrport.exe')
r=subprocess.run([str(exe),'--vertex-buffer-owners-control'],capture_output=True,text=True,timeout=30)
assert r.returncode==0,(r.returncode,r.stdout,r.stderr)
e=next(json.loads(x) for x in r.stdout.splitlines() if x.startswith('{') and 'vertex_buffer_owners_control' in x)
assert e['passed'] and e['source_mutation_isolated'] and e['failed_preflight_preserves_batch'] and e['identity_mismatch_rejected'] and e['stale_after_close']
Path('reports/reconstruction/infra/vertex_buffer_owners_control.json').write_text(json.dumps({'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'control':e,'R3':False,'engine_replacements':0},indent=2)+'\n');print(json.dumps(e))
