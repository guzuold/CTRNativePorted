# SPDX-License-Identifier: GPL-2.0-only
import subprocess,json,hashlib,datetime
from pathlib import Path
exe=Path('build/CTRPort-infra/ctrport.exe');controls=[]
for flag in ['--draw-packet-inputs-control','--draw-resource-inputs-control']:
 r=subprocess.run([str(exe),flag],capture_output=True,text=True,timeout=20)
 assert r.returncode==0,(flag,r.returncode,r.stdout,r.stderr)
 e=json.loads(r.stdout.strip());assert e['passed'];controls.append(e)
Path('reports/reconstruction/infra/draw_inputs_control.json').write_text(json.dumps({'timestamp':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'controls':controls,'R3':False,'engine_replacements':0},indent=2)+'\n');print(json.dumps(controls))
