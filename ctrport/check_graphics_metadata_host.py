# SPDX-License-Identifier: GPL-2.0-only
import subprocess,json,hashlib,datetime
from pathlib import Path
exe=Path('build/CTRPort-infra/ctrport.exe')
r=subprocess.run([str(exe),'--graphics-metadata-host-control'],capture_output=True,text=True,timeout=20)
assert r.returncode==0,(r.returncode,r.stdout,r.stderr)
e=json.loads(r.stdout.strip());assert e['passed'] and e['separate_host_stack'] and e['invalid_atomic']
Path('reports/reconstruction/infra/graphics_metadata_host_control.json').write_text(json.dumps({'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'control':e,'R3':False,'engine_replacements':0},indent=2)+'\n')
print(r.stdout.strip())
